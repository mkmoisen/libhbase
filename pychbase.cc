#include <Python.h>
#include "structmember.h"
#include <stdio.h>
#include <unistd.h>
#include <hbase/hbase.h>
#include <pthread.h>
#include <string.h>
#include <vector>


#if defined( WIN64 ) || defined( _WIN64 ) || defined( __WIN64__ ) || defined(_WIN32)
#define __WINDOWS__
#endif


#define CHECK(A)      \
    do {                \
        if (!(A)) {     \
            goto error; \
        }               \
    } while (0);


#define OOM_OBJ_RETURN_NULL(obj) \
    do {                        \
        if (!obj) {                 \
            return PyErr_NoMemory();  \
        }   \
    } while (0);

#define OOM_OBJ_RETURN_ERRNO(obj) \
    do {                        \
        if (!obj) {                 \
            return 12; \
        }   \
    } while (0);

#define OOM_ERRNO_RETURN_NULL(obj) \
    do {                        \
        if (obj == 12) {                 \
            return PyErr_NoMemory(); \
        }   \
    } while (0);

#define OOM_ERRNO_RETURN_ERRNO(obj) \
    do {                        \
        if (obj == 12) {                 \
            return 12; \
        }   \
    } while (0);

static PyObject *SpamError;
static PyObject *HBaseError;

typedef struct {
    // This is a macro, correct with no semi colon, which initializes fields to make it usable as a PyObject
    // Why not define first and last as char * ? Is there any benefit over each way?
    PyObject_HEAD
    PyObject *first;
    PyObject *last;
    int number;
    char *secret;
} Foo;

static void Foo_dealloc(Foo *self) {
    //dispose of your owned references
    //Py_XDECREF is sued because first/last could be NULL
    Py_XDECREF(self->first);
    Py_XDECREF(self->last);
    //call the class tp_free function to clean up the type itself.
    // Note how the Type is PyObject * insteaed of FooType * because the object may be a subclass
    self->ob_type->tp_free((PyObject *) self);

    // Note how there is no XDECREF on self->number
}

static PyObject *Foo_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    // Hm this isn't printing out?
    // Ok Foo_new isn't being called for some reason
    printf("In foo_new\n");
    Foo *self;// == NULL;
    // to_alloc allocates memory
    self = (Foo *)type->tp_alloc(type, 0);
    // One reason to implement a new method is to assure the initial values of instance variables
    // Here we are ensuring they initial values of first and last are not NULL.
    // If we don't care, we ould have used PyType_GenericNew() as the new method, which sets everything to NULL...
    if (self != NULL) {
        printf("in neww self is not null");
        self->first = PyString_FromString("");
        if (self->first == NULL) {
            Py_DECREF(self);
            return NULL;
        }

        self->last = PyString_FromString("");
        if (self->last == NULL) {
            Py_DECREF(self);
            return NULL;
        }

        self->number = 0;
    }


    // What about self->secret ?

    if (self->first == NULL) {
        printf("in new self first is null\n");
    } else {
        printf("in new self first is not null\n");
    }

    return (PyObject *) self;
}

static int Foo_init(Foo *self, PyObject *args, PyObject *kwargs) {
    //char *name;
    printf("In foo_init\n");
    PyObject *first, *last, *tmp;
    // Note how we can use &self->number, but not &self->first
    if (!PyArg_ParseTuple(args, "SSi", &first, &last, &self->number)) {
        //return NULL;
        return -1;
    }
    // What is the point of tmp?
    // The docs say we should always reassign members before decrementing their reference counts

    if (last) {
        tmp = self->last;
        Py_INCREF(last);
        self->last = last;
        Py_DECREF(tmp);
    }

    if (first) {
        tmp = self->first;
        Py_INCREF(first);
        self->first = first;
        //This was changed to DECREF from XDECREF once the get_first/last were set
        // This is because the get_first/last guarantee that it isn't null
        // but it caused a segmentation fault wtf?
        // Ok that was because the new method wasn't working bug
        Py_DECREF(tmp);
    }


    // Should I incref this?
    self->secret = "secret lol";
    printf("Finished foo_init");
    return 0;
}

/*
import pychbase
pychbase.Foo('a','b',5)
*/


// Make data available to Python
static PyMemberDef Foo_members[] = {
    //{"first", T_OBJECT_EX, offsetof(Foo, first), 0, "first name"},
    //{"last", T_OBJECT_EX, offsetof(Foo, last), 0, "last name"},
    {"number", T_INT, offsetof(Foo, number), 0, "number"},
    {NULL}
};

static PyObject *Foo_get_first(Foo *self, void *closure) {
    Py_INCREF(self->first);
    return self->first;
}

static int Foo_set_first(Foo *self, PyObject *value, void *closure) {
    printf("IN foo_set_first\n");
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the first attribute");
        return -1;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "The first attribute value must be a string");
        return -1;
    }

    Py_DECREF(self->first);
    Py_INCREF(value);
    self->first = value;
    printf("finished foo_set_first\n");
    return 0;
}

static PyObject *Foo_get_last(Foo *self, void *closure) {
    Py_INCREF(self->last);
    return self->last;
}

static int Foo_set_last(Foo *self, PyObject *value, void *closure) {
    printf("IN foo_set_last\n");
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the last attribute");
        return -1;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "The last attribute must be a string");
        return -1;
    }

    Py_DECREF(self->last);
    Py_INCREF(value);
    self->last = value;
    printf("finished foo_set_last\n");
    return 0;
}

static PyGetSetDef Foo_getseters[] = {
    {"first", (getter) Foo_get_first, (setter) Foo_set_first, "first name", NULL},
    {"last", (getter) Foo_get_last, (setter) Foo_set_last, "last name", NULL},
    {NULL}
};

static PyObject *Foo_square(Foo *self) {
    return Py_BuildValue("i", self->number * self->number);
}

static PyObject * Foo_name(Foo *self) {
    static PyObject *format = NULL;

    PyObject *args, *result;

    // We have to check for NULL, because they can be deleted, in which case they are set to NULL.
    // It would be better to prevent deletion of these attributes and to restrict the attribute values to strings.
    if (format == NULL) {
        format = PyString_FromString("%s %s");
        if (format == NULL) {
            return NULL;
        }
    }
    /*
    // These checks can be removed after adding the getter/setter that guarentees it cannot be null
    if (self->first == NULL) {
        PyErr_SetString(PyExc_AttributeError, "first");
        return NULL;
    }

    if (self->last == NULL) {
        PyErr_SetString(PyExc_AttributeError, "last");
        return NULL;
    }
    */

    args = Py_BuildValue("OO", self->first, self->last);
    if (args == NULL) {
        return NULL;
    }

    result = PyString_Format(format, args);
    // What is the difference between XDECREF and DECREF?
    // Use XDECREF if something can be null, DECREF if it is guarenteed to not be null
    Py_DECREF(args);

    return result;
}

// Make methods available
static PyMethodDef Foo_methods[] = {
    {"square", (PyCFunction) Foo_square, METH_VARARGS, "squares an int"},
    // METH_NOARGS indicates that this method should not be passed any arguments
    {"name", (PyCFunction) Foo_name, METH_NOARGS, "Returns the full name"},
    {NULL}
};

// Declare the type components
static PyTypeObject FooType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /* ob_size */
   "pychbase.Foo",               /* tp_name */
   sizeof(Foo),         /* tp_basicsize */
   0,                         /* tp_itemsize */
   (destructor)Foo_dealloc, /* tp_dealloc */
   0,                         /* tp_print */
   0,                         /* tp_getattr */
   0,                         /* tp_setattr */
   0,                         /* tp_compare */
   0,                         /* tp_repr */
   0,                         /* tp_as_number */
   0,                         /* tp_as_sequence */
   0,                         /* tp_as_mapping */
   0,                         /* tp_hash */
   0,                         /* tp_call */
   0,                         /* tp_str */
   0,                         /* tp_getattro */
   0,                         /* tp_setattro */
   0,                         /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "Foo object",        /* tp_doc */
   0,                         /* tp_traverse */
   0,                         /* tp_clear */
   0,                         /* tp_richcompare */
   0,                         /* tp_weaklistoffset */
   0,                         /* tp_iter */
   0,                         /* tp_iternext */
   Foo_methods,         /* tp_methods */
   Foo_members,         /* tp_members */
   Foo_getseters,                         /* tp_getset */
   0,                         /* tp_base */
   0,                         /* tp_dict */
   0,                         /* tp_descr_get */
   0,                         /* tp_descr_set */
   0,                         /* tp_dictoffset */
   (initproc)Foo_init,  /* tp_init */
   0,                         /* tp_alloc */
   Foo_new,                         /* tp_new */
};



// TODO eliminate these
static const char *cldbs = "hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222";
static const char *tableName = "/app/SubscriptionBillingPlatform/testInteractive";

/*
static const char *family1 = "Id";
static const char *col1_1  = "I";
static const char *family2 = "Name";
static const char *col2_1  = "First";
static const char *col2_2  = "Last";
static const char *family3 = "Address";
static const char *col3_1  = "City";
*/

/*
Given a family and a qualifier, return a fully qualified column (familiy + ":" + qualifier)
*/

static char *hbase_fqcolumn(const hb_cell_t *cell) {
    if (!cell) {
        return NULL;
    }
    char *family = (char *) cell->family;
    char *qualifier = (char *) cell->qualifier;

    int family_len = cell->family_len;
    int qualifier_len = cell->qualifier_len;


    // +1 for null terminator, +1 for colon
    //TODO This one is probably correct
    char *fq = (char *) malloc(1 + 1 + family_len + qualifier_len);
    if (!fq) {
        return NULL;
    }

    strncpy(fq, family, family_len);
    fq[family_len] = ':';
    fq[family_len + 1] = '\0';
    // strcat will replace the last null terminator before writing, then add a null terminator
    strncat(fq, qualifier, qualifier_len);

    return fq;
}


struct RowBuffer {
    std::vector<char *> allocedBufs;

    RowBuffer() {
        allocedBufs.clear();
    }

    ~RowBuffer() {
        while (allocedBufs.size() > 0) {
            char *buf = allocedBufs.back();
            allocedBufs.pop_back();
            delete [] buf;
            // It looks like these do the same thing? What is the difference between free and delete [] here?
            //free(buf);
        }
    }

    char *getBuffer(uint32_t size) {
        char *newAlloc = new char[size];
        allocedBufs.push_back(newAlloc);
        return newAlloc;
    }
    //PyObject *ret;
    //PyObject *rets;
};



/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.is_open()
connection.open()
connection.is_open()
connection.close()
connection.is_open()
*/



typedef struct {
    PyObject_HEAD
    PyObject *cldbs;
    // Add an is_open boolean
    bool is_open;
    hb_connection_t conn;
    hb_client_t client;
    hb_admin_t admin;
} Connection;

static void cl_dsc_cb(int32_t err, hb_client_t client, void *extra) {
    // Perhaps I could add a is_client_open boolean to connection ?
}

void admin_disconnection_callback(int32_t err, hb_admin_t admin, void *extra){
    printf("*****************************************************************admin_dc_cb: err = %d\n", err);
}

static PyObject *Connection_close(Connection *self) {
    if (self->is_open) {
        /*
        printf("In connection close\n");
        if (!self->admin) {
            printf("admin is null\n");
        } else {
            printf("admin is not null\n");
        }
        */
        //TODO this is causing a seg fault?
        //hb_admin_destroy(self->admin, admin_disconnection_callback, NULL);
        self->admin = NULL;
        hb_client_destroy(self->client, cl_dsc_cb, NULL);
        hb_connection_destroy(self->conn);
        self->is_open = false;
    }
    Py_RETURN_NONE;
}

static void Connection_dealloc(Connection *self) {
    Connection_close(self);
    //printf("after connection close\n");
    Py_XDECREF(self->cldbs);
    //printf("after xdecref cldbs\n");
    self->ob_type->tp_free((PyObject *) self);
    //printf("after tp_free\n");
    // I don't think I need to Py_XDECREF on conn and client?
}

// I'm going to skip Connection_new
// remember to FooType.tp_new = PyType_GenericNew;


static int Connection_init(Connection *self, PyObject *args, PyObject *kwargs) {
    PyObject *cldbs, *tmp;

    // Add an is_open boolean
    if (!PyArg_ParseTuple(args, "S", &cldbs)) {
        return -1;
    }

    // I'm not sure why tmp is necessary but it was in the docs
    tmp = self->cldbs;
    Py_INCREF(cldbs);
    self->cldbs = cldbs;
    Py_XDECREF(tmp);

    // Todo make CLDBS optional, and then find it from /opt/mapr/conf

    return 0;
}

static PyMemberDef Connection_members[] = {
    {"cldbs", T_OBJECT_EX, offsetof(Connection, cldbs), 0, "The cldbs connection string"},
    {NULL}
};

/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.is_open()
connection.open()
connection.is_open()
connection.close()
connection.is_open()
connection.close()

connection = pychbase._connection("abc")
connection.open()
connection.is_open()
connection.close()
connection.cldbs = "hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222"
connection.open()
connection.is_open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
*/
static PyObject *Connection_open(Connection *self) {
    if (!self->is_open) {
        int err = 0;
        err = hb_connection_create(PyString_AsString(self->cldbs), NULL, &self->conn);
        if (err != 0) {
            PyErr_Format(PyExc_ValueError, "Could not connect using CLDBS '%s': %i", self->cldbs, err);
            return NULL;
        }
        //OOM_OBJ_RETURN_NULL(self->conn);

        err = hb_client_create(self->conn, &self->client);
        OOM_OBJ_RETURN_NULL(self->client);
        if (err != 0) {
            PyErr_SetString(HBaseError, "Could not create client from connection");
            return NULL;
        }

        //("before admin create\n");
        err = hb_admin_create(self->conn, &self->admin);
        //printf("after admin create\n");
        OOM_OBJ_RETURN_NULL(self->admin);
        if (err != 0) {
            PyErr_SetString(PyExc_ValueError, "Could not create admin from connection");
            return NULL;
        }

        self->is_open = true;
    }
    Py_RETURN_NONE;

}




static PyObject *Connection_is_open(Connection *self) {
    if (self->is_open) {
        return Py_True;
    }
    return Py_False;
}


/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()
connection.create_table("/app/SubscriptionBillingPlatform/testpymaprdb21", {'f1': {}})


connection.create_table_wtf("/app/SubscriptionBillingPlatform/testpymaprdb20", {'f1': 'a'})




connection.create_table_wtf("/app/SubscriptionBillingPlatform/testpymaprdb11", ['hello'])


*/

static PyObject *Connection_delete_table(Connection *self, PyObject *args) {
    char *table_name;
    char *name_space = NULL;

    if (!PyArg_ParseTuple(args, "s|s", &table_name, &name_space)) {
        return NULL;
    }

    if (!self->is_open) {
        Connection_open(self);
    }


    int table_name_length = strlen(table_name);
    if (table_name_length > 1000) {
        PyErr_SetString(PyExc_ValueError, "Table name is too long\n");
        return NULL;
    }

    int err;
    //printf("before admin table exists\n");
    err = hb_admin_table_exists(self->admin, NULL, table_name);
    //printf("after admin table exists\n");
    if (err != 0) {
        PyErr_Format(PyExc_ValueError, "Table '%s' does not exist\n", table_name);
        return NULL;
    }
    //printf("before admin table delete\n");
    err = hb_admin_table_delete(self->admin, name_space, table_name);
    //printf("after admin table delete\n");
    if (err != 0) {
        PyErr_SetString(HBaseError, "Failed to delete table");
        return NULL;
    }



    Py_RETURN_NONE;
}

static PyObject *Connection_create_table(Connection *self, PyObject *args) {
    char *table_name;
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "sO!", &table_name, &PyDict_Type, &dict)) {
        return NULL;
    }

    if (!self->is_open) {
        Connection_open(self);
    }

    int err;
    //printf("after args\n");
    int table_name_length = strlen(table_name);
    // TODO verify the exact length at which this becomes illegal
    if (table_name_length > 1000) {
        PyErr_SetString(PyExc_ValueError, "Table name is too long\n");
        return NULL;
    }

    //printf("con create table before admin table exists\n");
    err = hb_admin_table_exists(self->admin, NULL, table_name);
    //printf("concreate table after admin table exists\n");
    if (err == 0) {
        PyErr_SetString(PyExc_ValueError, "Table already exists\n");
        return NULL;
    }

    PyObject *column_family_name;
    PyObject *column_family_attributes;
    Py_ssize_t i = 0;

    int number_of_families = PyDict_Size(dict);
    if (number_of_families < 1) {
        PyErr_SetString(PyExc_ValueError, "Need at least one column family");
        return NULL;
    }

    hb_columndesc families[number_of_families];

    int counter = 0;
    //printf("before outer loop\n");
    while (PyDict_Next(dict, &i, &column_family_name, &column_family_attributes)) {
        //printf("inner loop\n");

        char *column_family_name_char = PyString_AsString(column_family_name);
        OOM_OBJ_RETURN_NULL(column_family_name_char);

        err = hb_coldesc_create((byte_t *)column_family_name_char, strlen(column_family_name_char) + 1, &families[counter]);
        if (err != 0) {
            PyErr_Format(PyExc_ValueError, "Failed to create column descriptor '%s'", column_family_name_char);
            return NULL;
        }


        //families[i] = columndesc;

        if (!PyDict_Check(column_family_attributes)) {
            PyErr_SetString(PyExc_ValueError, "Attributes must be a dict");
            return NULL;
        };

        //Py_ssize_t dict_size = PyDict_Size(column_family_attributes);

        PyObject *key, *value;
        Py_ssize_t o = 0;
        //printf("before loop\n");
        while (PyDict_Next(column_family_attributes, &o, &key, &value)) {
            //printf("before looping\n");
            //if (!PyString_Check(key) && !PyUnicode_Check(key)) {
            if (!PyObject_TypeCheck(key, &PyBaseString_Type)) {
                PyErr_SetString(PyExc_ValueError, "Key must be string");
                return NULL;
            }
            if (!PyInt_Check(value)) {
                PyErr_SetString(PyExc_ValueError, "Value must be int");
                return NULL;
            }

            char *key_char = PyString_AsString(key);
            OOM_OBJ_RETURN_NULL(key_char);

            // TODO this can all be minimized as the key and value are always string and int respectively...
            if (strcmp(key_char, "max_versions") == 0) {
                int max_versions = PyInt_AsSsize_t(value);
                // error check?
                err = hb_coldesc_set_maxversions(families[counter], max_versions);
                if (err != 0) {
                    PyErr_Format(PyExc_ValueError, "Failed to add max version to column desc: %i", err);
                    return NULL;
                }
            } else if (strcmp(key_char, "min_versions") == 0) {
                int min_versions = PyInt_AsSsize_t(value);
                err = hb_coldesc_set_minversions(families[counter], min_versions);
                if (err != 0) {
                    PyErr_Format(PyExc_ValueError, "Failed to add min version to column desc: %i", err);
                    return NULL;
                }
            } else if (strcmp(key_char, "time_to_live") == 0) {
                int time_to_live = PyInt_AsSsize_t(value);
                err = hb_coldesc_set_ttl(families[counter], time_to_live);
                if (err != 0) {
                    PyErr_Format(PyExc_ValueError, "Failed to add time to live to column desc: %i", err);
                    return NULL;
                }
            } else if (strcmp(key_char, "in_memory") == 0) {
                int in_memory = PyInt_AsSsize_t(value);
                err = hb_coldesc_set_inmemory(families[counter], in_memory);
                if (err != 0) {
                    PyErr_Format(PyExc_ValueError, "Failed to add in memory to column desc: %i", err);
                    return NULL;
                }
            } else {
                PyErr_SetString(PyExc_ValueError, "Only max_versions, min_version, time_to_live, or in_memory permitted");
                return NULL;
            }

        }

        counter++;
    }
    //printf("before table_create\n");
    //printf("before con create table table create\n");
    err = hb_admin_table_create(self->admin, NULL, table_name, families, number_of_families);
    //printf("after con create table table create\n");
    //printf("after table_create\n");

    for (counter = 0; counter < number_of_families; counter++) {
        hb_coldesc_destroy(families[counter]);
    }


    if (err != 0) {
        //printf("err != 0\n");
        if (err == 36) {
            PyErr_SetString(PyExc_ValueError, "Table name is too long\n");
        } else {
            PyErr_Format(PyExc_ValueError, "Failed to admin table create: %i", err);
        }

        // Sometimes if it fails to create, the table still gets created but doesn't work?
        // Attempt to delete it
        //printf("before pybuildvalue\n");
        PyObject *table_name_obj = Py_BuildValue("(s)", table_name);
        OOM_OBJ_RETURN_NULL(table_name_obj);

        // I don't care if this succeeds or not
        //printf("before delete table\n");
        Connection_delete_table(self, table_name_obj);
        //printf("before return\n");

        return NULL;

    }

    Py_RETURN_NONE;
}


/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()
for i in range(1,20):
    try:
        connection.delete_table("/app/SubscriptionBillingPlatform/testpymaprdb{}".format(i))
    except ValueError:
        pass


*/




static PyMethodDef Connection_methods[] = {
    {"open", (PyCFunction) Connection_open, METH_NOARGS, "Opens the connection"},
    {"close", (PyCFunction) Connection_close, METH_NOARGS, "Closes the connection"},
    {"is_open", (PyCFunction) Connection_is_open, METH_NOARGS,"Checks if the connection is open"},
    {"create_table", (PyCFunction) Connection_create_table, METH_VARARGS, "Creates an HBase table"},
    {"delete_table", (PyCFunction) Connection_delete_table, METH_VARARGS, "Deletes an HBase table"},
    {NULL},
};

// Declare the type components
static PyTypeObject ConnectionType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /* ob_size */
   "pychbase._connection",               /* tp_name */
   sizeof(Connection),         /* tp_basicsize */
   0,                         /* tp_itemsize */
   (destructor)Connection_dealloc, /* tp_dealloc */
   0,                         /* tp_print */
   0,                         /* tp_getattr */
   0,                         /* tp_setattr */
   0,                         /* tp_compare */
   0,                         /* tp_repr */
   0,                         /* tp_as_number */
   0,                         /* tp_as_sequence */
   0,                         /* tp_as_mapping */
   0,                         /* tp_hash */
   0,                         /* tp_call */
   0,                         /* tp_str */
   0,                         /* tp_getattro */
   0,                         /* tp_setattro */
   0,                         /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "Connection object",        /* tp_doc */
   0,                         /* tp_traverse */
   0,                         /* tp_clear */
   0,                         /* tp_richcompare */
   0,                         /* tp_weaklistoffset */
   0,                         /* tp_iter */
   0,                         /* tp_iternext */
   Connection_methods,         /* tp_methods */
   Connection_members,         /* tp_members */
   0,                         /* tp_getset */
   0,                         /* tp_base */
   0,                         /* tp_dict */
   0,                         /* tp_descr_get */
   0,                         /* tp_descr_set */
   0,                         /* tp_dictoffset */
   (initproc)Connection_init,  /* tp_init */
   0,                         /* tp_alloc */
   0,                         /* tp_new */
};

/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.row('row-000')
connection.close()
table.row('row-000')
*/

typedef struct {
    PyObject_HEAD
    Connection *connection;
    // Do I need to INCREF/DECREF this since I am exposing it to the python layer?
    // Is it better or worse taht this is char * instead of PyObject * ?
    char *table_name;
} Table;

/*
The HBase C API uses callbacks for everything.
The callbacks will increment the table->count, which is used to track if the call back finished
This CallBackBuffer holds a reference to both the table and to the row buf
The call back needs to free the row buf and increment the count when its done
*/

struct BatchCallBackBuffer;

struct CallBackBuffer {
    RowBuffer *rowBuf;
    Table *table;
    int err;
    PyObject *ret;
    uint64_t count;
    pthread_mutex_t mutex;
    BatchCallBackBuffer *batch_call_back_buffer;
    //PyObject *rets;
    CallBackBuffer(Table *t, RowBuffer *r, BatchCallBackBuffer *bcbb) {
        table = t;
        rowBuf = r;
        err = 0;
        count = 0;
        batch_call_back_buffer = bcbb;
        mutex = PTHREAD_MUTEX_INITIALIZER;
        ret = NULL;
    }
    ~CallBackBuffer() {
        /*
        // rowBuf is now being deleting inside the put/delete callbacks
        // Note that the rowBuf must absolutely be deleted in all exit scenarios or else it will lead to a m
        // memory leak because I have removed the deletion from this destructor
        if (rowBuf) {
            printf("rowBuf is not null, deleting it\n");
            delete rowBuf;
            printf("After delete rowBuf\n");
        }
        */

    }
};

/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.batch([], 10000)
*/
struct BatchCallBackBuffer {
    //CallBackBuffer *call_back_buffers;
    std::vector<CallBackBuffer *> call_back_buffers;
    int number_of_mutations;
    int count;
    int errors;
    pthread_mutex_t mutex;

    BatchCallBackBuffer(int i) {
        number_of_mutations = i;
        call_back_buffers.reserve(i);
        count = 0;
        errors = 0;
        // TODO compiler gives warnings about this check it out
        mutex = PTHREAD_MUTEX_INITIALIZER;
    }

    ~BatchCallBackBuffer() {
        //delete call_back_buffers;
        //printf("In Batch Call Back Buffer\n");
        while (call_back_buffers.size() > 0) {
            //printf("Calling .back()\n");
            CallBackBuffer *call_back_buffer = call_back_buffers.back();

            //printf("popping back\n");
            call_back_buffers.pop_back();
            //printf("deleting back\n");
            delete call_back_buffer; // In row buffer destructor, its delete [] buf ...
            //printf("after delete\n");
            // doesn't work
            //free(call_back_buffers);
        }
        //printf("end destructor\n");
        // doesn't work
        //free(call_back_buffers);

    }

};


static void Table_dealloc(Table *self) {
    Py_XDECREF(self->connection);
    self->ob_type->tp_free((PyObject *) self);
}

/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteracasdfasdtive')
*/
static int Table_init(Table *self, PyObject *args, PyObject *kwargs) {
    Connection *connection, *tmp;
    char *table_name = NULL;

    if (!PyArg_ParseTuple(args, "O!s", &ConnectionType ,&connection, &table_name)) {
        return -1;
    }

    if (!connection->is_open) {
        Connection_open(connection);
    }

    //int err = hb_admin_table_exists(self->connection->admin, NULL, self->table_name);
    //printf("before table_init create table table create\n");
    int err = hb_admin_table_exists(connection->admin, NULL, table_name);
    //printf("after table_init create table table create\n");
    if (err != 0) {
        // Apparently in INIT methods I have to return -1, NOT NULL or else it won't work properly
        PyErr_Format(PyExc_ValueError, "Table '%s' does not exist", table_name);
        //return NULL; // return err;
        return -1;
    }

    // Oddly, if I set self->connection before the above error check/raise exception
    // If I make a table() that fails because the table doesn't exist
    // The next time I touch connection I get a seg fault?
    // I don't understand the point of the tmp here but they did it in the docs...
    self->table_name = table_name;
    tmp = self->connection;
    Py_INCREF(connection);
    self->connection = connection;
    Py_XDECREF(tmp);

    return 0;
}

// TODO Should I prevent the user from changing the name of the table as it will have no effect?
// Or should changing the name actually change the table?

static PyMemberDef Table_members[] = {
    {"table_name", T_STRING, offsetof(Table, table_name), 0, "The name of the MapRDB table"},
    {NULL}
};


static int read_result(hb_result_t result, PyObject *dict) {
    int err = 0;

    OOM_OBJ_RETURN_ERRNO(result);
    OOM_OBJ_RETURN_ERRNO(dict);

    size_t cellCount = 0;

    // Do I need to error check this?
    hb_result_get_cell_count(result, &cellCount);

    for (size_t i = 0; i < cellCount; ++i) {
        const hb_cell_t *cell;
         // Do I need to error check this?
        hb_result_get_cell_at(result, i, &cell);
        OOM_OBJ_RETURN_ERRNO(cell);

        int value_len = cell->value_len;
        char *value_cell = (char *) cell->value;
        char *value_char = (char *) malloc(1 + value_len);
        OOM_OBJ_RETURN_ERRNO(value_char);
        strncpy(value_char, value_cell, value_len);
        value_char[value_len] = '\0';


        // Set item steals the ref right? No need to INC/DEC?
        // No it doesn't https://docs.python.org/2/c-api/dict.html?highlight=pydict_setitem#c.PyDict_SetItem
        //Py_BuildValue() may run out of memory, and this should be checked
        // Hm I'm not sure if I have to decref Py_BuildValue for %s, Maybe its only %O
        // http://stackoverflow.com/questions/5508904/c-extension-in-python-return-py-buildvalue-memory-leak-problem
        // TODO Does Py_BuildValue copy in the contents or take the pointer? hbase_fqcolumn is mallocing a pointer and returning the pointer...
        // For now I'll free it a few lines down
        char *fq = hbase_fqcolumn(cell);
        if (!fq) {
            free(value_char);
            return 12; //ENOMEM Cannot allocate memory
        }

        PyObject *key = Py_BuildValue("s", fq);
        free(fq);
        if (!key) {
            free(value_char);
            return 12; //ENOMEM Cannot allocate memory
        }

        PyObject *value = Py_BuildValue("s", value_char);
        free(value_char);
        if (!value) {
            Py_DECREF(key);
            return 12; //ENOMEM Cannot allocate memory
        }

        //printf("keys ref count is %i\n", key->ob_refcnt);
        err = PyDict_SetItem(dict, key, value);
        if (err != 0) {
            // Is this check necessary?
            Py_DECREF(key);
            Py_DECREF(value);
            return err;
        }
        //printf("keys ref count after set item is %i\n", key->ob_refcnt);
        // TODO Do I need to decref key and value? Yes I do
        Py_DECREF(key);
        Py_DECREF(value);

    }

    return err;
}

/*
Make absolutely certain that the count is set to 1 in all possible exit scenarios
Or else the calling function will hang.
*/
/*
* It's very important to delete the RowBuf in all possible cases in this call back
* or else it will result in a memory leak
*/
static void row_callback(int32_t err, hb_client_t client, hb_get_t get, hb_result_t result, void *extra) {
    // What should I do if this is null?
    // There is no way to set the count and it will just hang.
    // I suppose its better to crash the program?
    // Maybe if there was some global count I could increment and check for?
    // TODO consider that option^
    //printf("in row_callback\n");
    CallBackBuffer *call_back_buffer = (CallBackBuffer *) extra;
    //printf("before err != 0\n");
    if (err != 0) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->err = err;
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        return;
    }

    //printf("!result\n");
    if (!result) {
        // Note that if there is no row for the rowkey, result is not NULL
        // I doubt err wouldn't be 0 if result is null
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->err = 12;
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        return;
    }

    //const byte_t *key;
    //size_t keyLen;
    // This returns the rowkey even if there is no row for this rowkey
    //hb_result_get_key(result, &key, &keyLen);
    //printf("key is %s\n", key);

    // Do I need to dec ref? I don't know, memory isn't increasing when i run this in a loop
    //printf("before pydict new\n");
    PyObject *dict = PyDict_New();
    if (!dict) {
        //printf("dict is null\n");
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->err = 12;
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        return;
    }
    //printf("before read result\n");
    err = read_result(result, dict);
    //printf("after read result\n");
    if (err != 0) {
        //printf("read result was %i", call_back_buffer->err);
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->err = err;
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        return;
    }

    //printf("before final lock\n");
    pthread_mutex_lock(&call_back_buffer->mutex);
    call_back_buffer->ret = dict;
    call_back_buffer->count = 1;
    delete call_back_buffer->rowBuf;
    pthread_mutex_unlock(&call_back_buffer->mutex);
    //printf("after final lock\n");

    hb_result_destroy(result);
    //printf("after destroy\n");
    hb_get_destroy(get);
    //printf("after get destory\n");
}
/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.row('hello')
*/

/*
This has a memory leak:
top -b | grep python

import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
print table.table_name
table.row('hello')
print table.table_name

# TODO LOL REALLY WEIRD BUG IF I LEAVE THE COMMENT IN TABLE NAME GETS CHANGED???

while True:
    # Leaks for both no result and for result
    table.row('hello')
*/

static PyObject *Table_row(Table *self, PyObject *args) {
    //printf("In table_row\n");
    char *row_key;

    if (!PyArg_ParseTuple(args, "s", &row_key)) {
        return NULL;
    }
    //printf("before self->connection\n");
    if (!self->connection->is_open) {
        Connection_open(self->connection);
    }

    int err = 0;

    //printf("beforeget_create\n");
    hb_get_t get;
    err = hb_get_create((const byte_t *)row_key, strlen(row_key), &get);
    OOM_OBJ_RETURN_NULL(get);
    if (err != 0) {
        PyErr_Format(HBaseError, "Could not create get with row key '%s'", row_key);
        return NULL;
    }
    //printf("after self->connection\n");

    err = hb_get_set_table(get, self->table_name, strlen(self->table_name));
    if (err != 0) {
        PyErr_Format(PyExc_ValueError, "Could not set table name '%s' on get", self->table_name);
        return NULL;
    }

    //printf("after set table\n");

    RowBuffer *row_buff = new RowBuffer();
    OOM_OBJ_RETURN_NULL(row_buff);

    CallBackBuffer *call_back_buffer = new CallBackBuffer(self, row_buff, NULL);
    // TODO delete this and add deelte row_buf
    OOM_OBJ_RETURN_NULL(call_back_buffer);

    //printf("before get send\n");
    err = hb_get_send(self->connection->client, get, row_callback, call_back_buffer);
    //printf("get send err is %i\n", err);
    if (err != 0) {
        delete row_buff;
        delete call_back_buffer;
        PyErr_Format(HBaseError, "Could not send get: %i", err);
        return NULL;
    }

    //printf("before wait\n");
    //sleep(5);
    // TODO Do I need to lock this?
    uint64_t local_count = 0;
    //while (call_back_buffer->count != 1) {
    while (local_count != 1) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        local_count = call_back_buffer->count;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        sleep(0.1);
    }

    //return self->ret;
    //printf("before callback_buffer->wait\n");
    PyObject *ret = call_back_buffer->ret;

    err = call_back_buffer->err;

    //printf("before delete call_back_buffer\n");
    delete call_back_buffer;

    if (err != 0) {
        PyErr_Format(PyExc_ValueError, "Get failed: %i", err);
        return NULL;
    }
    //printf("before ret\n");
    return ret;
}


void client_flush_callback(int32_t err, hb_client_t client, void *ctx) {
    //printf("Client flush callback invoked: %d\n", err);
    // TODO should I add a buffer to *ctx anddo something with it
}


/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.put("snoop", {"f:foo": "bar"})
*/
// TODO Document this and change its name
// TODO Document err codes
static int split(char *fq, char *family, char *qualifier) {
    OOM_OBJ_RETURN_ERRNO(fq);

    int i = 0;
    // Initialize family to length, + 1 for null pointer, - 1 for the colon
    bool found_colon = false;

    // this should either be strlen(fq) - 1, or strlen(fq) without the fq[i] != '\0' right?
    for (i = 0; i < strlen(fq) && fq[i] != '\0'; i++) {
        if (fq[i] != ':') {
            family[i] = fq[i];
        } else {
            found_colon = true;
            break;
        }
    }

    if (!found_colon) {
        return -10;
    }

    family[i] = '\0';

    // This works with strlen(..) + 1 or without + 1 ... why ??
    int qualifier_index = 0;
    for (i=i + 1; i < strlen(fq) && fq[i] != '\0'; i++) {
        qualifier[qualifier_index] = fq[i];
        qualifier_index += 1;
    }
    qualifier[qualifier_index] = '\0';

    return 0;

}




/*
* It's very important to delete the RowBuf in all possible cases in this call back
* or else it will result in a memory leak
*/
void put_callback(int err, hb_client_t client, hb_mutation_t mutation, hb_result_t result, void *extra) {
    // TODO hb_mutation_set_bufferable
    /*

    http://tsunanet.net/~tsuna/asynchbase/api/org/hbase/async/PutRequest.html#setBufferable(boolean)
    Sets whether or not this RPC is can be buffered on the client side. The default is true.

    Setting this to false bypasses the client-side buffering, which is used to send RPCs in batches for greater throughput, and causes this RPC to be sent directly to the server.

    Parameters:
    bufferable - Whether or not this RPC can be buffered (i.e. delayed) before being sent out to HBase.

     * Sets whether or not this RPC can be buffered on the client side.
     *
     * Currently only puts and deletes can be buffered. Calling this for
     * any other mutation type will return EINVAL.
     *
     * The default is true.

    HBASE_API int32_t
    hb_mutation_set_bufferable(
        hb_mutation_t mutation,
        const bool bufferable);
     */

    // TODO Check types.h for the HBase error codes

    // TODO Dont error check this or else it will hang forever
    CallBackBuffer *call_back_buffer = (CallBackBuffer *) extra;

    if (err != 0) {
        //printf("MapR API Failed on Put Callback %i\n", err);
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->count = 1;
        call_back_buffer->err = err;
        delete call_back_buffer->rowBuf;
        if (call_back_buffer->batch_call_back_buffer) {
            pthread_mutex_lock(&call_back_buffer->batch_call_back_buffer->mutex);
            call_back_buffer->batch_call_back_buffer->errors++;
            call_back_buffer->batch_call_back_buffer->count++;
            pthread_mutex_unlock(&call_back_buffer->batch_call_back_buffer->mutex);
        }
        pthread_mutex_unlock(&call_back_buffer->mutex);
        return;
    }


    // It looks like result is always NULL for put?

    pthread_mutex_lock(&call_back_buffer->mutex);
    call_back_buffer->count = 1;
    // TODO If I comment this the seg fault goes away ...
    delete call_back_buffer->rowBuf;
    if (call_back_buffer->batch_call_back_buffer) {
        pthread_mutex_lock(&call_back_buffer->batch_call_back_buffer->mutex);
        call_back_buffer->batch_call_back_buffer->count++;
        pthread_mutex_unlock(&call_back_buffer->batch_call_back_buffer->mutex);
    }
    pthread_mutex_unlock(&call_back_buffer->mutex);

    hb_mutation_destroy(mutation);

}

static int create_dummy_cell(hb_cell_t **cell,
                      const char *r, size_t rLen,
                      const char *f, size_t fLen,
                      const char *q, size_t qLen,
                      const char *v, size_t vLen) {
    // Do I need to check this
    hb_cell_t *cell_ptr = new hb_cell_t();
    OOM_OBJ_RETURN_ERRNO(cell_ptr);

    cell_ptr->row = (byte_t *)r;
    cell_ptr->row_len = rLen;

    cell_ptr->family = (byte_t *)f;
    cell_ptr->family_len = fLen;

    cell_ptr->qualifier = (byte_t *)q;
    cell_ptr->qualifier_len = qLen;

    cell_ptr->value = (byte_t *)v;
    cell_ptr->value_len = vLen;

    *cell = cell_ptr;

    return 0;
}


/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.put('snoop', {'Name:a':'a','Name:foo':'bar'})
for i in range(1000000):
    table.put('snoop', {'Name:a':'a','Name:foo':'bar'})

lol()
*/


// TODO Document error codes for user error
// split returns -10 if no colon was found

static int make_put(Table *self, RowBuffer *row_buf, const char *row_key, PyObject *dict, hb_put_t *hb_put, bool is_bufferable) {
    int err;

    OOM_OBJ_RETURN_ERRNO(self);
    OOM_OBJ_RETURN_ERRNO(row_buf);
    OOM_OBJ_RETURN_ERRNO(row_key);
    OOM_OBJ_RETURN_ERRNO(dict);

    int size = PyDict_Size(dict);
    if (size < 1) {
        return -5;
    }

    err = hb_put_create((byte_t *)row_key, strlen(row_key), hb_put);
    OOM_OBJ_RETURN_ERRNO(hb_put);
    if (err != 0) {
        return err;
    }

    PyObject *fq, *value;
    Py_ssize_t pos = 0;
    hb_cell_t *cell;

    // https://docs.python.org/2/c-api/dict.html?highlight=pydict_next#c.PyDict_Next
    // This says PyDict_Next borrows references for key and value...
    while (PyDict_Next(dict, &pos, &fq, &value)) {

        // Its weird if I loop batch with 100000, the ref count is 100002 for value??

        char *fq_char = PyString_AsString(fq);

        OOM_OBJ_RETURN_ERRNO(fq_char);
        if (strlen(fq_char) == 0) {
            //printf("Null or empty fq\n");
            return -1;
        }

        char *family = row_buf->getBuffer(strlen(fq_char)); // Don't +1 for null terminator, because of colon
        OOM_OBJ_RETURN_ERRNO(family);

        char *qualifier = row_buf->getBuffer(strlen(fq_char)); // Don't +1 for null terminator, because of colon
        OOM_OBJ_RETURN_ERRNO(family);

        err = split(fq_char, family, qualifier);
        if (err != 0) {
            return err;
        }

        char *value_char = PyString_AsString(value);
        OOM_OBJ_RETURN_ERRNO(value_char);
        // I suppose an empty string here is OK

        //char *v = row_buf->getBuffer(strlen(value_char));
        //OOM_OBJ_RETURN_ERRNO(v);

        // No errors when I replace v with value_char in create_dummy_cell..
        // I'm under the impression I need to add the family and qualifier to some buffer until it successfully flushes
        // Whereas value_char doesn't require this since its still being stored in memory via python..?
        // Then in the call back can't I delete the buffer for family/qualifier?
        //strcpy(v, value_char);

        //err = create_dummy_cell(&cell, row_key, strlen(row_key), family, strlen(family), qualifier, strlen(qualifier), v, strlen(v));
        //printf("before dummy cell\n");
        err = create_dummy_cell(&cell, row_key, strlen(row_key), family, strlen(family), qualifier, strlen(qualifier), value_char, strlen(value_char));
        OOM_ERRNO_RETURN_ERRNO(err);
        if (err != 0) {
            return err;
        }

        err = hb_put_add_cell(*hb_put, cell);;
        if (err != 0) {
            //PyErr_Format(PyExc_ValueError, "Could not add cell to put: %i", err);
            return err;
        }

        delete cell;
    }

    err = hb_mutation_set_table((hb_mutation_t)*hb_put, self->table_name, strlen(self->table_name));
    if (err != 0) {
        // Is it dangerous to pass in self->table_name here if its null?
        //PyErr_Format(PyExc_ValueError, "Could not put's table for '%s': %i", self->table_name, %i);
        return err;
    }
    /*
    err = hb_mutation_set_bufferable((hb_mutation_t)*hb_put, is_bufferable);
    if (err != 0) {
        // Is it dangerous to pass in self->table_name here if its null?
        //PyErr_Format(PyExc_ValueError, "Could not put's table for '%s': %i", self->table_name, %i);
        return err;
    }
    */

    return err;
}

static PyObject *Table_put(Table *self, PyObject *args) {
    char *row_key;
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "sO!", &row_key, &PyDict_Type, &dict)) {
        return NULL;
    }
    //printf("Table_put dict ref count is %i\n", dict->ob_refcnt);

    int err = 0;

    RowBuffer *row_buf = new RowBuffer();
    OOM_OBJ_RETURN_NULL(row_buf);

    CallBackBuffer *call_back_buffer = new CallBackBuffer(self, row_buf, NULL);
    OOM_OBJ_RETURN_NULL(call_back_buffer);
    //if (!call_back_buffer) {
    //    delete row_buf;
    //    return PyErr_NoMemory();
    //}

    hb_put_t hb_put;
    //printf("before make put\n");
    // TODO activate is bufferable
    err = make_put(self, row_buf, row_key, dict, &hb_put, true);
    //printf("after make_put dict's ref count is %i\n", dict->ob_refcnt);
    //OOM_OBJ_RETURN_NULL(hb_put);
    /*
    if (!hb_put) {
        //printf("hb_put is null\n");
        delete row_buf;
        delete call_back_buffer;
        //printf("before return\n");
        return PyErr_NoMemory();
    }
    */
    //printf("after hb_put check\n");
    //OOM_OBJ_RETURN_NULL(hb_put);
    if (err != 0) {
        delete row_buf;
        delete call_back_buffer;
        // This would just override the error message set in make_put
        // PyErr_SetString(PyExc_ValueError, "Could not create put oh noo");
        // TODO A cool feature would be to let the user specify column families (since the API doesn't let me figure it out)
        // And then fail if they try to add it to a batched put
        if (err == -10) {
            PyErr_SetString(PyExc_ValueError, "All keys must contain a colon delimiting the family and qualifier");
        } else if (err == -5) {
            // TODO Should I really fail here? Why not just take no action?
            PyErr_SetString(PyExc_ValueError, "Put dictionary was empty");

        } else if (err == -1) {
            PyErr_SetString(PyExc_ValueError, "Column Qualifier was empty");
        } else {
            // Hmm would it still be user error at this point?
            PyErr_Format(PyExc_ValueError, "Failed to make put: %i", err);
        }
        // TODO Add more error cases for ValueError from make_put
        return NULL;
    }

    //printf("before mutation send\n");
    // https://github.com/mapr/libhbase/blob/0ddda015113452955ed600116f58a47eebe3b24a/src/main/native/jni_impl/hbase_client.cc#L151
    // https://github.com/mapr/libhbase/blob/0ddda015113452955ed600116f58a47eebe3b24a/src/main/native/jni_impl/hbase_client.cc#L151
    // https://github.com/mapr/libhbase/blob/0ddda015113452955ed600116f58a47eebe3b24a/src/main/native/jni_impl/hbase_client.cc#L268
    // https://github.com/mapr/libhbase/blob/0ddda015113452955ed600116f58a47eebe3b24a/src/main/native/jni_impl/jnihelper.h#L73
    // It looks like the following happens:
    // If I submit a null client or hb_put, hb_mutation invokes the call back BUT sets the errno
    //      Then the result of hb_mutation_send is actually a 0!
    //
    // I suppose the only time hb_mutation_send returns non-0 is if there is an issue in JNI_GET_ENV
    //      This issue would prevent the sendGet from ever happening, as well as the callback
    // So a non-0 means that the call back has not been invoked and its safe to delete rowbuf?
    //
    // Ya so if err is not 0, call back has not been invoked, and its safe/necessary to delete row_buf
    err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_put, put_callback, call_back_buffer);
    if (err != 0) {
        delete row_buf;
        delete call_back_buffer;
        PyErr_Format(HBaseError, "Put failed to send: %i", err);
        return NULL;
    }
    //printf("after mutation send\n");

    /*
    If client is null, flush will still invoke the callback and set the errno in call back
    Same as wet mutation_send/get_send, the only time an error is returned if the JNI ENV couldn't be set
    and its guarenteed that the flush won't execute the call back
    ... however, since the mutation_send in the above step was successful, doesn't this imply that I
    cannot delete row_buf here?

    oh ok one major subetly to be aware of:
        If hb_mutation buffering is OFF, the above hb_mutation MAY OR MAY NOT have sent
        if hb_mutation buffering is ON, I dont think the above hb_mutation will have sent

    Actually, it appears that the buffering doesn't work or that there is something I'm missing.
    If I set buffering on or not, it still ends up being sent before the flush?
    */
    err = hb_client_flush(self->connection->client, client_flush_callback, NULL);
    if (err != 0) {
        // callback will have deleted the row buf
        delete call_back_buffer;
        PyErr_Format(HBaseError, "Put failed to flush: %i", err);
        //return NULL; // lol this was commented before UNCOMMENT THIS
    }

    //uint64_t locCount;
    //do {
    //    sleep (1);
    //    pthread_mutex_lock(&mutex);
    //    locCount = count;
    //    pthread_mutex_unlock(&mutex);
    //} while (locCount < numRows);

    // TODO Do I need to aquire the lock on this?
    // Yes I do this fixed the seg fault! I wonder why..
    uint64_t local_count = 0;
    while (local_count != 1) {
    //while (call_back_buffer->count != 1) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        local_count = call_back_buffer->count;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        sleep(0.1);
    }
    //printf("after wait\n");

    err = call_back_buffer->err;
    //err = 0;
    //printf("after call err\n");

    //printf("after delete rowbuf\n");
    // TODO COMMENTING THIS LINE STOPS THE SEGFAULT
    delete call_back_buffer;
    // call_back_buffer->row_buf is deleting in callback
    //printf("after deletes\n");
    if (err != 0) {
        if (err == 2) {
            PyErr_Format(PyExc_ValueError, "Put failed; probably bad column family: %i", err);
        } else {
            PyErr_Format(HBaseError, "Put Failed: %i");
        }

        return NULL;
    }

    Py_RETURN_NONE;
}

/*
* Remember to delete the rowBuf in all possible exit cases or else it will leak memory
*/
// TODO I should re returning -1 for all the return; statements here!
// But this is void...
void scan_callback(int32_t err, hb_scanner_t scan, hb_result_t *results, size_t numResults, void *extra) {
    // TODO I think its better to segfault to prevent hanging
    CallBackBuffer *call_back_buffer = (CallBackBuffer *) extra;

    if (err != 0) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->err = err;
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        return;
    }
    if (!results) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->err = 12;
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        return;
    }

    if (numResults > 0) {
        PyObject *dict;

        for (uint32_t r = 0; r < numResults; ++r) {
            const byte_t *key;
            size_t keyLen;

            // API doesn't document when this returns something other than 0
            err = hb_result_get_key(results[r], &key, &keyLen);
            if (err != 0) {
                pthread_mutex_lock(&call_back_buffer->mutex);
                call_back_buffer->err = err;
                call_back_buffer->count = 1;
                delete call_back_buffer->rowBuf;
                pthread_mutex_unlock(&call_back_buffer->mutex);
                return;
            }



            // Do I need a null check?
            dict = PyDict_New();
            if (!dict) {
                pthread_mutex_lock(&call_back_buffer->mutex);
                call_back_buffer->err = 12;
                call_back_buffer->count = 1;
                delete call_back_buffer->rowBuf;
                pthread_mutex_unlock(&call_back_buffer->mutex);
                return;
            }

            // I cannot imagine this lock being necessary
            //pthread_mutex_lock(&call_back_buffer->mutex);
            err = read_result(results[r], dict);
            //pthread_mutex_unlock(&call_back_buffer->mutex);
            if (err != 0) {
                pthread_mutex_lock(&call_back_buffer->mutex);
                call_back_buffer->err = err;
                call_back_buffer->count = 1;
                delete call_back_buffer->rowBuf;
                pthread_mutex_unlock(&call_back_buffer->mutex);
                Py_DECREF(dict);
                // TODO If I decref this will i seg fault if i access it later?
                // Should it be set to a none?
                return;
            }

            //printf("dicts ref count after read_results %i\n", dict->ob_refcnt);
            //printf("before append\n");
            // Do I need to INCREF the result of Py_BuildValue?
            // Should I do that ! with the type? Does that make it faster or slower lol

            char *key_char = (char *) malloc(1 + keyLen);
            strncpy(key_char, (char *)key, keyLen);
            key_char[keyLen] = '\0';

            PyObject *tuple = Py_BuildValue("sO",(char *)key_char, dict);
            free(key_char);

            if (!tuple) {
                pthread_mutex_lock(&call_back_buffer->mutex);
                call_back_buffer->err = 12;
                call_back_buffer->count = 1;
                delete call_back_buffer->rowBuf;
                pthread_mutex_unlock(&call_back_buffer->mutex);
                Py_DECREF(dict);
                return;
            }

            // I can't imagine this lock being necessary
            // However the helgrind report went from 24000 lines to 3500 after adding it?
            pthread_mutex_lock(&call_back_buffer->mutex);
            err = PyList_Append(call_back_buffer->ret, tuple);
            pthread_mutex_unlock(&call_back_buffer->mutex);
            if (err != 0) {
                pthread_mutex_lock(&call_back_buffer->mutex);
                call_back_buffer->err = err;
                call_back_buffer->count = 1;
                delete call_back_buffer->rowBuf;
                pthread_mutex_unlock(&call_back_buffer->mutex);
                Py_DECREF(dict);
                Py_DECREF(tuple);
                // TODO If I decref this will i seg fault if i access it later?
                // Should itb e set to a none?
                return;
            }
            //printf("dicts ref count after append %i\n", dict->ob_refcnt);

            Py_DECREF(dict);
            //printf("dicts ref count after decref %i", dict->ob_refcnt);

            // Do I need to decref tuple?

            hb_result_destroy(results[r]);
        }
        // The API doesn't specify when the return value would not be 0
        // But it is used in this unittest:
        // https://github.com/mapr/libhbase/blob/0ddda015113452955ed600116f58a47eebe3b24a/src/test/native/unittests/libhbaseutil.cc#L760 // Valgrind shows a possible data race write of size 1 by one thread to a previous write of size 1 by a different thread, both on the following line...// I cannot lock this though right?
        err = hb_scanner_next(scan, scan_callback, call_back_buffer);
        if (err != 0) {
            //PyErr_SetString(PyExc_ValueError, "Failed in scanner callback");
            pthread_mutex_lock(&call_back_buffer->mutex);
            call_back_buffer->err = err;
            call_back_buffer->count = 1;
            delete call_back_buffer->rowBuf;
            pthread_mutex_unlock(&call_back_buffer->mutex);
            return;
        }
    } else {
        //sleep(0.1);
        // TODO test this to see if the callback is executed for a scan with no results ... it should print here right
        // TODO Is it necessary to aquire the lock here? Isn't there only going to be one thread on this?
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;
        pthread_mutex_unlock(&call_back_buffer->mutex);
    }
}

/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.scan('hello', 'hello100~')
*/

static PyObject *Table_scan(Table *self, PyObject *args) {
    char *start = "";
    char *stop = "";

    if (!PyArg_ParseTuple(args, "|ss", &start, &stop)) {
        return NULL;
    }

    int err = 0;

    hb_scanner_t scan;
    err = hb_scanner_create(self->connection->client, &scan);
    if (err != 0) {
        PyErr_Format(HBaseError, "Failed to create the scanner: %i", err);
        return NULL;
    }

    err = hb_scanner_set_table(scan, self->table_name, strlen(self->table_name));
    if (err != 0) {
        // TODO I should probably verify that nothing will go wrong in the event self->table_name is NULL
        PyErr_Format(PyExc_ValueError, "Failed to set table '%s' on scanner: %i", self->table_name, err);
        return NULL;
    }

    // TODO parameratize this
    err = hb_scanner_set_num_versions(scan, 1);
    if (err != 0) {
        PyErr_Format(HBaseError, "Failed to set num versions on scanner: %i", err);
        return NULL;
    }

    if (strlen(start) > 0) {
        err = hb_scanner_set_start_row(scan, (byte_t *) start, strlen(start));
        if (err != 0) {
            // ValueError as I am assuming this is a user error in the row key value
            PyErr_Format(PyExc_ValueError, "Failed to set start row on scanner: %i", err);
            return NULL;
        }
    }
    if (strlen(stop) > 1) {
        err = hb_scanner_set_end_row(scan, (byte_t *) stop, strlen(stop));
        if (err != 0) {
            // ValueError as I am assuming this is a user error in the row key value
            PyErr_Format(PyExc_ValueError, "Failed to set stop row on scanner: %i", err);
            return NULL;
        }
    }

    // Does it optimize if I set this higher?
    // TODO what is this?
    /**
     * Sets the maximum number of rows to scan per call to hb_scanner_next().
     */
    // TODO Ok oddly in the sample code they use 1 or 3 for this value. Shouldn't I set it really high? or 0????
    err = hb_scanner_set_num_max_rows(scan, 1);
    if (err != 0) {
        PyErr_Format(HBaseError, "Failed to set num_max_rows scanner: %i", err);
        return NULL;
    }


    RowBuffer *row_buf = new RowBuffer();
    OOM_OBJ_RETURN_NULL(row_buf);

    CallBackBuffer *call_back_buffer = new CallBackBuffer(self, row_buf, NULL);
    // TODO replace this and delete row_buf
    OOM_OBJ_RETURN_NULL(call_back_buffer);

    call_back_buffer->ret = PyList_New(0);
    // TODO replcae this and delete call_back_buffer
    OOM_OBJ_RETURN_NULL(call_back_buffer->ret);


    err = hb_scanner_next(scan, scan_callback, call_back_buffer);
    if (err != 0) {
        PyErr_Format(HBaseError, "Scan failed: %i", err);
        // TODO do I need to delete anything ??
        return NULL;
    }
    uint64_t local_count = 0;
    //while (call_back_buffer->count != 1) {
    while (local_count != 1) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        local_count = call_back_buffer->count;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        sleep(0.1);
    }

    // TODO I need to free this right
    PyObject *ret = call_back_buffer->ret;
    //printf("ret has ref count of %i\n", ret->ob_refcnt);
    err = call_back_buffer->err;
    delete call_back_buffer;
    //printf("after delete call back buffer has ref count of %i\n", ret->ob_refcnt);

    if (err != 0) {
        PyErr_Format(HBaseError, "Scan failed: %i", err);
        return NULL;
    }

    return ret;
}

/*
* It's very important to delete the RowBuf in all possible cases in this call back
* or else it will result in a memory leak
*/
void delete_callback(int err, hb_client_t client, hb_mutation_t mutation, hb_result_t result, void *extra) {
    // It looks like result is always NULL for delete?

    // TODO In the extraordinary event that this is null, is it better to just segfault as its such an extreme bug?
    CallBackBuffer *call_back_buffer = (CallBackBuffer *) extra;

    if (err != 0) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        call_back_buffer->err = err;
        call_back_buffer->count = 1;
        delete call_back_buffer->rowBuf;


        if (call_back_buffer->batch_call_back_buffer) {
            pthread_mutex_lock(&call_back_buffer->batch_call_back_buffer->mutex);
            call_back_buffer->batch_call_back_buffer->errors++;
            call_back_buffer->batch_call_back_buffer->count++;
            pthread_mutex_unlock(&call_back_buffer->batch_call_back_buffer->mutex);
        }
        pthread_mutex_unlock(&call_back_buffer->mutex);

        return;
    }

    pthread_mutex_lock(&call_back_buffer->mutex);
    call_back_buffer->count = 1;
    delete call_back_buffer->rowBuf;
    if (call_back_buffer->batch_call_back_buffer) {
        pthread_mutex_lock(&call_back_buffer->batch_call_back_buffer->mutex);
        call_back_buffer->batch_call_back_buffer->count++;
        pthread_mutex_unlock(&call_back_buffer->batch_call_back_buffer->mutex);
    }
    pthread_mutex_unlock(&call_back_buffer->mutex);
    
    hb_mutation_destroy(mutation);

}

/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.row('hello1')
table.delete('hello1')
*/

static int make_delete(Table *self, char *row_key, hb_delete_t *hb_delete) {
    int err = 0;

    OOM_OBJ_RETURN_ERRNO(self);
    OOM_OBJ_RETURN_ERRNO(row_key);
    // TODO I shouldn't check hb_delete for null right?

    if (strlen(row_key) == 0) {
        err = -5;
        return err;
    }

    err = hb_delete_create((byte_t *)row_key, strlen(row_key), hb_delete);
    if (err != 0) {
        return err;
    }
    OOM_OBJ_RETURN_ERRNO(hb_delete);

    err = hb_mutation_set_table((hb_mutation_t)*hb_delete, self->table_name, strlen(self->table_name));
    if (err != 0) {
        return err;
    }

    return err;
}

static PyObject *Table_delete(Table *self, PyObject *args) {
    char *row_key;

    if (!PyArg_ParseTuple(args, "s", &row_key)) {
        return NULL;
    }
    if (!self->connection->is_open) {
        Connection_open(self->connection);
    }

    int err = 0;

    // TODO Do I need to check to see if hb_delete is null inside of the make_delete function?
    hb_delete_t hb_delete;
    err = make_delete(self, row_key, &hb_delete);
    OOM_ERRNO_RETURN_NULL(err);
    if (err != 0) {
        if (err == -5) {
            PyErr_SetString(PyExc_ValueError, "row_key was empty string");
            return NULL;
        } else {
            PyErr_Format(PyExc_ValueError, "Failed to create Delete with rowkey '%s' or set it's Table with '%s': %i", row_key, self->table_name, err);
        }
    }

    // I'm not even using the row_buf for deletes
    RowBuffer *row_buf = new RowBuffer();
    OOM_OBJ_RETURN_NULL(row_buf);

    CallBackBuffer *call_back_buffer = new CallBackBuffer(self, row_buf, NULL);
    // TODO replace this and delete row_buf
    OOM_OBJ_RETURN_NULL(call_back_buffer);

    err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_delete, delete_callback, call_back_buffer);
    if (err != 0) {
        delete row_buf;
        delete call_back_buffer;
        PyErr_SetString(HBaseError, "Delete failed to send and may not have succeeded");
        return NULL;
    }

    err = hb_client_flush(self->connection->client, client_flush_callback, NULL);
    if (err != 0) {
        delete call_back_buffer;
        PyErr_SetString(HBaseError, "Delete failed to flush and may not have succeeded");
        return NULL;
    }
    // TODO do I need to lock this?
    uint64_t local_count = 0;
    while (local_count != 1) {
        pthread_mutex_lock(&call_back_buffer->mutex);
        local_count = call_back_buffer->count;
        pthread_mutex_unlock(&call_back_buffer->mutex);
        sleep(0.1);
    }

    err = call_back_buffer->err;

    delete call_back_buffer;

    if (err != 0) {
        PyErr_SetString(HBaseError, "Delete may have failed");
        return NULL;
    }

    Py_RETURN_NONE;
}


/*
import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.batch([('put', 'hello{}'.format(i), {'f:bar':'bar{}'.format(i)}) for i in range(100000)])
#table.scan()


import pychbase
connection = pychbase._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = pychbase._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.batch([('delete', 'hello{}'.format(i), {'Name:bar':'bar{}'.format(i)}) for i in range(100000)])



table.batch([], 10000)
table.batch([None for _ in range(1000000)], 10)
table.batch([('delete', 'hello{}'.format(i)) for i in range(100000)])

*/


static PyObject *Table_batch(Table *self, PyObject *args) {
    PyObject *actions;
    PyObject *is_bufferable = NULL;
    //printf("before arg\n");
    if (!PyArg_ParseTuple(args, "O!|O!", &PyList_Type, &actions, &PyBool_Type, &is_bufferable)) {
        return NULL;
    }
    //printf("after args\n");

    bool is_bufferable_bool = true;

    if (is_bufferable) {
        if (!PyObject_IsTrue(is_bufferable)) {
            is_bufferable_bool = false;
        }
    }
    //printf("after is bufferable\n");

    int err;
    int number_of_actions = PyList_Size(actions);
    PyObject *tuple;
    Py_ssize_t i;

    // TODO If in the future I return the results, set the PyList_new(number_of_actions);
    PyObject *results = PyList_New(0);
    OOM_OBJ_RETURN_NULL(results);

    BatchCallBackBuffer *batch_call_back_buffer = new BatchCallBackBuffer(number_of_actions);
    // TODO I need to PyDecref results .. Maybe the macro can have varrying args?
    OOM_OBJ_RETURN_NULL(batch_call_back_buffer);

    //printf("Before loop\n");
    for (i = 0; i < number_of_actions; i++) {
        //printf("looping\n");
        RowBuffer *rowBuf = new RowBuffer();
        if (!rowBuf) {
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);
            continue;
        }

        CallBackBuffer *call_back_buffer = new CallBackBuffer(self, rowBuf, batch_call_back_buffer);
        if (!call_back_buffer) {
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            delete rowBuf;

            continue;
        }

        batch_call_back_buffer->call_back_buffers.push_back(call_back_buffer);

        tuple = PyList_GetItem(actions, i); // borrows reference
        if (!tuple) {
            // Is this check even necessary? Docs say it is  Borrowed Reference
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = 12;

            delete rowBuf;

            continue;
        }

        if (!PyTuple_Check(tuple)) {
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = -1; //TODO BETTER

            delete rowBuf;

            continue;
        }

        PyObject *mutation_type = PyTuple_GetItem(tuple, 0);
        if (!mutation_type) {
            // Is this check even necessary
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = 12;

            delete rowBuf;

            continue;
        }
        //if (!PyString_Check(mutation_type) && !PyUnicode_Check(mutation_type)) {
        if (!PyObject_TypeCheck(mutation_type, &PyBaseString_Type)) {
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = -1; //TODO BETTER

            delete rowBuf;

            continue;
        }
        char *mutation_type_char = PyString_AsString(mutation_type);
        if (!mutation_type_char) {
            // Is this check even necessary
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = 12;

            delete rowBuf;

            continue;
        }

        PyObject *row_key = PyTuple_GetItem(tuple, 1);
        if (!row_key) {
            // Is this check even necessary
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = 12;

            delete rowBuf;

            continue;
        }

        //if (!PyString_Check(row_key) && !PyUnicode_Check(row_key)) {
        if (!PyObject_TypeCheck(row_key, &PyBaseString_Type)) {
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = -1; //TODO BETTER

            delete rowBuf;

            continue;
        }

        char *row_key_char = PyString_AsString(row_key);
        if (!row_key_char) {
            // Is this check even necessary
            // Docs seem to indicate it is not https://docs.python.org/2/c-api/string.html#c.PyString_AsString
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = 12;

            delete rowBuf;

            continue;
        }

        if (strcmp(mutation_type_char, "put") == 0) {
            //printf("size of call_back_buffers is %ld\n",sizeof(batch_call_back_buffer->call_back_buffers));
            //In particular, all functions whose function it is to create a new object, such as PyInt_FromLong() and Py_BuildValue(), pass ownership to the receiver.
            //printf("tuples ref count after pystringasstring 1 is %i\n", tuple->ob_refcnt);
            PyObject *dict = PyTuple_GetItem(tuple, 2);
            if (!dict) {
                // Is this check even necessary
                pthread_mutex_lock(&batch_call_back_buffer->mutex);
                batch_call_back_buffer->errors++;
                batch_call_back_buffer->count++;
                pthread_mutex_unlock(&batch_call_back_buffer->mutex);

                call_back_buffer->count++;
                call_back_buffer->err = 12;

                delete rowBuf;

                continue;
            }
            if (!PyDict_Check(dict)) {
                pthread_mutex_lock(&batch_call_back_buffer->mutex);
                batch_call_back_buffer->errors++;
                batch_call_back_buffer->count++;
                pthread_mutex_unlock(&batch_call_back_buffer->mutex);

                call_back_buffer->count++;
                call_back_buffer->err = -1;

                delete rowBuf;

                continue;
            }

            // do I need to increment dict? or decrement it?
            // do I need to destroy hb_put on erors?
            hb_put_t hb_put;
            err = make_put(self, rowBuf, row_key_char, dict, &hb_put, is_bufferable_bool);
            //printf("dict ref count after make put %i\n", dict->ob_refcnt);
            if (err != 0) {
                pthread_mutex_lock(&batch_call_back_buffer->mutex);
                batch_call_back_buffer->errors++;
                batch_call_back_buffer->count++;
                pthread_mutex_unlock(&batch_call_back_buffer->mutex);

                call_back_buffer->count++;
                call_back_buffer->err = err;

                delete rowBuf;

                continue;
            }
            // The only time hb_mutation_send results in non-zero means the call back has NOT been invoked
            // So its safe to delete rowBuf
            err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_put, put_callback, call_back_buffer);
            //printf("dict ref count after send %i\n", dict->ob_refcnt);
            // TODO ADD the hb_put to the call back buffer and free it!
            if (err != 0) {
                // TODO do I need to hb_mutation_destroy(hb_put) ?
                pthread_mutex_lock(&batch_call_back_buffer->mutex);
                batch_call_back_buffer->errors++;
                batch_call_back_buffer->count++;
                pthread_mutex_unlock(&batch_call_back_buffer->mutex);

                pthread_mutex_lock(&call_back_buffer->mutex);
                call_back_buffer->count++;
                if (call_back_buffer->err == 0) {
                    call_back_buffer->err = err;

                    delete rowBuf;
                }
                pthread_mutex_unlock(&call_back_buffer->mutex);

                continue;
            }

        } else if (strcmp(mutation_type_char, "delete") == 0) {
            hb_delete_t hb_delete;
            err = make_delete(self, row_key_char, &hb_delete);
            if (err != 0) {
                pthread_mutex_lock(&batch_call_back_buffer->mutex);
                batch_call_back_buffer->errors++;
                batch_call_back_buffer->count++;
                pthread_mutex_unlock(&batch_call_back_buffer->mutex);

                call_back_buffer->count++;
                call_back_buffer->err = err;

                delete rowBuf;

                continue;
            }
            err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_delete, delete_callback, call_back_buffer);
            if (err != 0) {
                // Do I need to destroy the mutation if send fails?
                pthread_mutex_lock(&batch_call_back_buffer->mutex);
                batch_call_back_buffer->errors++;
                batch_call_back_buffer->count++;
                pthread_mutex_unlock(&batch_call_back_buffer->mutex);

                pthread_mutex_lock(&call_back_buffer->mutex);
                call_back_buffer->count++;
                if (call_back_buffer->err == 0) {
                    call_back_buffer->err = err;

                    delete rowBuf;
                }
                pthread_mutex_unlock(&call_back_buffer->mutex);

                continue;
            }
        } else {
            // Must be put or delete
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            batch_call_back_buffer->errors++;
            batch_call_back_buffer->count++;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);

            call_back_buffer->count++;
            call_back_buffer->err = -1; //TODO BETTER

            delete rowBuf;

            continue;
        }
    }

    //printf("done with loop going to flush\n");

    if (number_of_actions > 0) {

        //self->count = 0;
        // TODO Oh no ... The docs say:
        // TODO Note that this doesn't guarantee that ALL outstanding RPCs have completed.
        // TODO Need to figure out the implications of this...
        //printf("SLEEPING FOR 10 BEFORE FLUSH\n");
        //sleep(10);
        //printf("before flush\n");
        err = hb_client_flush(self->connection->client, client_flush_callback, NULL);
        //printf("after flush, SLEEPING FOR 10\n");
        //sleep(10);
        //printf("after sleep\n");
        if (err != 0) {
            //printf("we have errors\n");
            // The documentation doesn't specify if this would ever return an error or why.
            // If this fails with an error and the call back is never invoked, my script would hang..
            // I'll temporarily raise an error until I can clarify this
            PyErr_SetString(HBaseError, "Flush failed. Batch may be partially committed");
            return NULL;
        }
        //printf("Waiting for all callbacks to return ...\n");


        //while (self->count < number_of_actions) {
        // TODO do I need to lock this?
        uint64_t local_count = 0;
        //while (batch_call_back_buffer->count < number_of_actions) {
        while (local_count < number_of_actions) {
            pthread_mutex_lock(&batch_call_back_buffer->mutex);
            local_count = batch_call_back_buffer->count;
            pthread_mutex_unlock(&batch_call_back_buffer->mutex);
            // TODO this sleep should be optimized based on the number of actions?
            // E.g. perhaps at most 1 full second is OK if the number of actions is large enough?
            sleep(0.1);
        }
    }

    int errors = batch_call_back_buffer->errors;


    if (errors > 0) {
        // TODO I should really go through and get the results and give them back to user
    }

    //printf("Before delete batch_call_back_buffer\n");
    delete batch_call_back_buffer;
    //printf("after delete batch_call_back_buffer\n");
    //printf("wait was %ld\n", wait);
    PyObject *ret_tuple = Py_BuildValue("iO", errors, results);
    OOM_OBJ_RETURN_NULL(ret_tuple);

    Py_DECREF(results);

    return ret_tuple;
}


static PyMethodDef Table_methods[] = {
    {"row", (PyCFunction) Table_row, METH_VARARGS, "Gets one row"},
    {"put", (PyCFunction) Table_put, METH_VARARGS, "Puts one row"},
    {"scan", (PyCFunction) Table_scan, METH_VARARGS, "Scans the table"},
    {"delete", (PyCFunction) Table_delete, METH_VARARGS, "Deletes one row"},
    {"batch", (PyCFunction) Table_batch, METH_VARARGS, "sends a batch"},
    {NULL}
};

// Declare the type components
static PyTypeObject TableType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /* ob_size */
   "pychbase._table",               /* tp_name */
   sizeof(Table),         /* tp_basicsize */
   0,                         /* tp_itemsize */
   (destructor)Table_dealloc, /* tp_dealloc */
   0,                         /* tp_print */
   0,                         /* tp_getattr */
   0,                         /* tp_setattr */
   0,                         /* tp_compare */
   0,                         /* tp_repr */
   0,                         /* tp_as_number */
   0,                         /* tp_as_sequence */
   0,                         /* tp_as_mapping */
   0,                         /* tp_hash */
   0,                         /* tp_call */
   0,                         /* tp_str */
   0,                         /* tp_getattro */
   0,                         /* tp_setattro */
   0,                         /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "Connection object",        /* tp_doc */
   0,                         /* tp_traverse */
   0,                         /* tp_clear */
   0,                         /* tp_richcompare */
   0,                         /* tp_weaklistoffset */
   0,                         /* tp_iter */
   0,                         /* tp_iternext */
   Table_methods,         /* tp_methods */
   Table_members,         /* tp_members */
   0,                         /* tp_getset */
   0,                         /* tp_base */
   0,                         /* tp_dict */
   0,                         /* tp_descr_get */
   0,                         /* tp_descr_set */
   0,                         /* tp_dictoffset */
   (initproc)Table_init,  /* tp_init */
   0,                         /* tp_alloc */
   PyType_GenericNew,                         /* tp_new */
};





// The C function always has self and args
// for Module functions, self is NULL; for a method, self is the object
static PyObject *pychbase_system(PyObject *self, PyObject *args)
{
    const char *command;
    int sts;
    //PyArg_ParseTuple converts the python arguments to C values
    // It returns if all arguments are valid
    if (!PyArg_ParseTuple(args, "s", &command))
        // Returning NULL throws an exception
        return NULL;
    sts = system(command);
    if (sts < 0) {
        // Note how this sets the exception, and THEN returns null!
        PyErr_SetString(SpamError, "System command failed");
        return NULL;
    }
    return PyLong_FromLong(sts);
}
/*
from _pychbase import *
import sys
lol = 'noob'
sys.getrefcount(lol)
py_buildvalue_char(lol)
sys.getrefcount(lol)
*/
static PyObject *py_buildvalue_char(PyObject *self, PyObject *args) {
    char *row_key;
    if (!PyArg_ParseTuple(args, "s", &row_key)) {
        return NULL;
    }

    //printf("row_key ref count is %i\n", row_key->ob_refcnt);
    //char *row_key_char = PyString_AsString(row_key);
    //printf("row_key ref count is now %i\n", row_key->ob_refcnt);

    PyObject *row_key_obj;
    row_key_obj = Py_BuildValue("s", row_key);
    printf("row_key_obj ref count is now %i\n", row_key_obj->ob_refcnt);
    //Py_INCREF(row_key_obj);
    // It looks like I have to decref this if I'm not going to be retuning it
    //printf("row_key_obj is now %i\n", row_key_obj->ob_refcnt);
    // ref count is 1, so Py_BuildValue("s", ...) doesn't increase the refcnt?
    //Py_DECREF(row_key_obj);

    PyObject *dict = PyDict_New();
    printf("dict ref count %i\n", dict->ob_refcnt);

    PyObject *key = Py_BuildValue("s", "foo");
    printf("key ref count is %i\n", key->ob_refcnt);

    PyDict_SetItem(dict, key, row_key_obj);
    printf("after set item\n");
    printf("dict ref count %i\n", dict->ob_refcnt);
    printf("key ref count is %i\n", key->ob_refcnt);
    printf("row_key_obj ref count is now %i\n", row_key_obj->ob_refcnt);
    Py_DECREF(key);
    Py_DECREF(row_key_obj);
    printf("after decrefs\n");
    printf("dict ref count %i\n", dict->ob_refcnt);
    printf("key ref count is %i\n", key->ob_refcnt);
    printf("row_key_obj ref count is now %i\n", row_key_obj->ob_refcnt);


    //PyObject *tuple;
    //printf("tuple ref count is %i\n", tuple->ob_refcnt);
    //tuple = Py_BuildValue("(O)", row_key_obj);
    //printf("row_key_obj ref count is now %i\n", row_key_obj->ob_refcnt);
    // ref count is 2, so Py_BuildValue("(O)", ...) increfds the rec on the O
    //printf("tuple ref count is now %i\n", tuple->ob_refcnt);
    //ref count here is 1, so the tuples ref count doesn't increase

    Py_RETURN_NONE;
    //return tuple;
}

static PyObject *lol(PyObject *self, PyObject *args) {
    printf("Noob\n");
    // This is how to write a void method in python
    Py_RETURN_NONE;
}

static void noob(char *row_key) {
    printf("you are a noob");
    char rk[100];
    printf("Before segmentation fault");
    strcpy(rk, row_key);
    printf("After segmentation fault");
}
/*
static PyObject *get(PyObject *self, PyObject *args) {
    char *row_key;
    if (!PyArg_ParseTuple(args, "s", &row_key)) {
        return NULL;
    }
    Connection *connection = new Connection();
    printf("hai I am %s\n", row_key);
    printf("before test_get\n");
    PyObject *lol = pymaprdb_get(connection, tableName, row_key);
    printf("done with foo\n");
    delete connection;
    //noob(row_key);
    return lol;
}
*/
/*
import pychbase
pychbase.put('hai', {'Name:First': 'Matthew'})
*/


/*
import pychbase
pychbase.scan()
*/



static PyObject *build_int(PyObject *self, PyObject *args) {
    return Py_BuildValue("i", 123);
}

static PyObject *build_dict(PyObject *self, PyObject *args) {
    return Py_BuildValue("{s:i}", "name", 123);
}

static PyObject *add_to_dict(PyObject *self, PyObject *args) {
    PyObject *key;
    PyObject *value;
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "OOO", &dict, &key, &value)) {
        return NULL;
    }

    printf("Parsed successfully\n");

    PyDict_SetItem(dict, key, value);

    Py_RETURN_NONE;
}

static PyObject *print_dict(PyObject *self, PyObject *args) {
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "O!", &PyDict_Type, &dict)) {
        return NULL;
    }

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value)) {
        //PyString_AsString converts a PyObject to char * (and assumes it is actually a char * not some other data type)

        printf("key is %s\n", PyString_AsString(key));
        printf("value is %s\n", PyString_AsString(value));
    }

    Py_RETURN_NONE;

}

static PyObject *build_list(PyObject *self, PyObject *args) {
    int num;
    if (!PyArg_ParseTuple(args, "i", &num)) {
        return NULL;
    }
    printf("num is %i\n", num);
    PyObject *list = PyList_New(0);
    int i = 0;
    for (i = 0; i < num; i++) {
        PyObject *val = Py_BuildValue("s", "hai");
        PyList_Append(list, val);
        // This doesn't seem to help?
        Py_DECREF(val);
    }

    return list;
}

/*
static PyObject *super_dict(PyObject *self, PyObject *args) {
    char *f1;
    char *k1;
    char *v1;
    char *f2;
    char *k2;
    char *v2;

    if (!PyArg_ParseTuple(args, "ssssss", &f1, &k1, &v1, &f2, &k2, &v2)) {
        return NULL;
    }
    printf("f1 is %s\n", f1);
    printf("k1 is %s\n", k1);
    printf("v1 is %s\n", v1);
    printf("f2 is %s\n", f2);
    printf("k2 is %s\n", k2);
    printf("v2 is %s\n", v2);

    //char *first = (char *) malloc(1 + 1 + strlen(f1) + strlen(f2));
    //strcpy(first, f1);
    //first[strlen(f1)] = ':';
    //strcat(first, k1);


    // somehow take args as a tuple
    PyObject *dict = PyDict_New();

    char *first = hbase_fqcolumn(f1, k1);
    if (!first) {
            return NULL;//ENOMEM Cannot allocate memory
        }
    char *second = hbase_fqcolumn(f2, k2);
    if (!second) {
            return NULL;//ENOMEM Cannot allocate memory
    }

    printf("First is %s\n", first);
    printf("Second is %s\n", second);

    PyDict_SetItem(dict, Py_BuildValue("s", first), Py_BuildValue("s", v1));
    free(first);
    PyDict_SetItem(dict, Py_BuildValue("s", second), Py_BuildValue("s", v2));
    free(second);

    return dict;
}
*/

static PyObject *print_list(PyObject *self, PyObject *args) {
    //PyListObject seems to suck, it isn't accepted by PyList_Size for example
    PyObject *actions;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &actions)) {
        return NULL;
    }

    //http://effbot.org/zone/python-capi-sequences.htm
    // This guy recommends PySequence_Fast api
    PyObject *value;
    Py_ssize_t i;
    for (i = 0; i < PyList_Size(actions); i++) {
        value = PyList_GetItem(actions, i);
        printf("value is %s\n", PyString_AsString(value));
    }

    Py_RETURN_NONE;
}

/*
import pychbase
pychbase.print_list_t([('put', 'row1', {'a':'b'}), ('delete', 'row2')])
*/
static PyObject *print_list_t(PyObject *self, PyObject *args) {
    //PyListObject seems to suck, it isn't accepted by PyList_Size for example
    PyObject *actions;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &actions)) {
        return NULL;
    }

    //http://effbot.org/zone/python-capi-sequences.htm
    // This guy recommends PySequence_Fast api

    PyObject *tuple;
    Py_ssize_t i;
    for (i = 0; i < PyList_Size(actions); i++) {
        tuple = PyList_GetItem(actions, i);
        printf("got tuple\n");
        char *mutation_type = PyString_AsString(PyTuple_GetItem(tuple, 0));
        printf("got mutation_type\n");
        printf("mutation type is %s\n", mutation_type);
        if (strcmp(mutation_type, "put") == 0) {
            printf("Its a put");
        } else if (strcmp(mutation_type, "delete") == 0) {
            printf("its a delete");
        }
    }

    Py_RETURN_NONE;
}

/*
import string
import pychbase
pychbase.print_list([c for c in string.letters])
*/
static PyObject *print_list_fast(PyObject *self, PyObject *args) {
    //http://effbot.org/zone/python-capi-sequences.htm
    // This guy says the PySqeunce_Fast api is faster
    // hm later on he says You can also use the PyList API (dead link), but that only works for lists, and is only marginally faster than the PySequence_Fast API.
    PyObject *actions;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &actions)) {
        return NULL;
    }

    PyObject *seq;
    int i, len;

    PyObject *value;

    seq = PySequence_Fast(actions, "expected a sequence");
    len = PySequence_Size(actions);

    for (i = 0; i < len; i++) {
        value = PySequence_Fast_GET_ITEM(seq, i);
        printf("Value is %s\n", PyString_AsString(value));
    }

    Py_RETURN_NONE;


}




/*
lol = pychbase.build_dict()
print lol
pychbase.add_to_dict(lol, 'hai', 'bai')

lol = pychbase.


import pychbase
pychbase.super_dict('f', 'k1', 'v1', 'f2', 'k2', 'v2')

*/
/*
static PyObject *foo(PyObject *self, PyObject *args) {
    int lol = pymaprdb_get(NULL);
    Py_RETURN_NONE;
}
*/

static PyMethodDef SpamMethods[] = {
    {"system",  pychbase_system, METH_VARARGS, "Execute a shell command."},
    {"lol", lol, METH_VARARGS, "your a lol"},
    //{"get", get, METH_VARARGS, "gets a row given a rowkey"},
    //{"put", put, METH_VARARGS, "puts a row and dict"},
    //{"scan", scan, METH_VARARGS, "scans"},
    {"build_int", build_int, METH_VARARGS, "build an int"},
    {"build_dict", build_dict, METH_VARARGS, "build a dict"},
    {"add_to_dict", add_to_dict, METH_VARARGS, "add to dict"},
    //{"super_dict", super_dict, METH_VARARGS, "super dict"},
    {"print_dict", print_dict, METH_VARARGS, "print dict"},
    {"build_list", build_list, METH_VARARGS, "build list"},
    {"print_list", print_list, METH_VARARGS, "prints a list"},
    {"print_list_fast", print_list_fast, METH_VARARGS, "prints a list using the fast api"},
    {"print_list_t", print_list_t, METH_VARARGS, "pritns a list of tuples"},
    {"py_buildvalue_char", py_buildvalue_char, METH_VARARGS, "build value string"},
    {NULL, NULL, 0, NULL}
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_pychbase(void)
{
    PyObject *m;

    m = Py_InitModule("_pychbase", SpamMethods);
    if (m == NULL) {
        return;
    }

    // Fill in some slots in the type and make it ready
    // I suppose I use this if I don't write my own new mthod?
    //FooType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&FooType) < 0) {
        return;
    }

    if (PyType_Ready(&ConnectionType) < 0) {
        return;
    }

    if (PyType_Ready(&TableType) < 0) {
        return;
    }


    // no tp_new here because its in the FooType
    Py_INCREF(&FooType);
    PyModule_AddObject(m, "Foo", (PyObject *) &FooType);

    // Add the type to the module
    // failing to add this tp_new will result in: TypeError: cannot create 'pychbase._connection' instances
    ConnectionType.tp_new = PyType_GenericNew;
    Py_INCREF(&ConnectionType);
    PyModule_AddObject(m, "_connection", (PyObject *) &ConnectionType);

    //TableType.tp_new = PyType_GenericNew;
    Py_INCREF(&TableType);
    PyModule_AddObject(m, "_table", (PyObject *) &TableType);

    SpamError = PyErr_NewException("pychbase.error", NULL, NULL);
    Py_INCREF(SpamError);
    PyModule_AddObject(m, "error", SpamError);

    HBaseError = PyErr_NewException("pychbase.HBaseError", NULL, NULL);
    Py_INCREF(HBaseError);
    PyModule_AddObject(m, "HBaseError", HBaseError);
}

int
main(int argc, char *argv[])
{

    Py_SetProgramName(argv[0]);


    Py_Initialize();


    init_pychbase();
}