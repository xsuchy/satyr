#include "common.h"
#include "strbuf.h"

static PyMethodDef ThreadMethods[] = {
    /* getters & setters */
    { "get_number",     p_btp_thread_get_number,     METH_NOARGS,  t_get_number_doc     },
    { "set_number",     p_btp_thread_set_number,     METH_VARARGS, t_set_number_doc     },
    /* methods */
    { "cmp",            p_btp_thread_cmp,            METH_VARARGS, t_cmp_doc            },
    { "dup",            p_btp_thread_dup,            METH_NOARGS,  t_dup_doc            },
    { "quality_counts", p_btp_thread_quality_counts, METH_NOARGS,  t_quality_counts_doc },
    { "quality",        p_btp_thread_quality,        METH_NOARGS,  t_quality_doc        },
    { NULL },
};

static PyMemberDef ThreadMembers[] = {
    { (char *)"frames", T_OBJECT_EX, offsetof(ThreadObject, frames), 0, t_frames_doc },
    { NULL },
};

PyTypeObject ThreadTypeObject = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Thread",          /* tp_name */
    sizeof(ThreadObject),       /* tp_basicsize */
    0,                          /* tp_itemsize */
    p_btp_thread_free,          /* tp_dealloc */
    NULL,                       /* tp_print */
    NULL,                       /* tp_getattr */
    NULL,                       /* tp_setattr */
    NULL,                       /* tp_compare */
    NULL,                       /* tp_repr */
    NULL,                       /* tp_as_number */
    NULL,                       /* tp_as_sequence */
    NULL,                       /* tp_as_mapping */
    NULL,                       /* tp_hash */
    NULL,                       /* tp_call */
    p_btp_thread_str,           /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    thread_doc,                 /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    ThreadMethods,              /* tp_methods */
    ThreadMembers,              /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    p_btp_thread_new,           /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* helpers */
int thread_prepare_linked_list(ThreadObject *thread)
{
    int i;
    PyObject *item;
    FrameObject *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(thread->frames); ++i)
    {
        item = PyList_GetItem(thread->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);

        if (!PyObject_TypeCheck(item, &FrameTypeObject))
        {
            Py_XDECREF(item);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError, "frames must be a list of btparser.Frame objects");
            return -1;
        }

        current = (FrameObject *)item;
        if (i == 0)
            thread->thread->frames = current->frame;
        else
            prev->frame->next = current->frame;

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        current->frame->next = NULL;
        Py_DECREF(current);
    }

    return 0;
}

PyObject *thread_prepare_frame_list(struct btp_thread *thread)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;

    struct btp_frame *frame = thread->frames;
    FrameObject *item;
    while (frame)
    {
        item = (FrameObject *)PyObject_MALLOC(sizeof(FrameTypeObject));
        if (!item)
            return PyErr_NoMemory();
        PyObject_INIT(item, &FrameTypeObject);
        Py_INCREF(item);

        item->frame = frame;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        frame = frame->next;
    }
    return result;
}

/* constructor */
PyObject *p_btp_thread_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    ThreadObject *to = (ThreadObject *)PyObject_MALLOC(sizeof(ThreadObject));
    if (!to)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    PyObject_INIT(to, &ThreadTypeObject);
    if (str)
    {
        struct btp_location location;
        btp_location_init(&location);
        to->thread = btp_thread_parse(&str, &location);
        if (!to->thread)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
        to->frames = thread_prepare_frame_list(to->thread);
        if (!to->frames)
            return NULL;
        Py_INCREF(to->frames);
    }
    else
    {
        to->frames = PyList_New(0);
        to->thread = btp_thread_new();
    }

    return (PyObject *)to;
}

/* destructor */
void p_btp_thread_free(PyObject *object)
{
    ThreadObject *this = (ThreadObject *)object;
    this->thread->frames = NULL;
    btp_thread_free(this->thread);
    PyObject_Del(object);
}

PyObject *p_btp_thread_str(PyObject *self)
{
    ThreadObject *this = (ThreadObject *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Thread #%u with %d frames",
                           this->thread->number,
                           PyList_Size(this->frames));
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */
PyObject *p_btp_thread_get_number(PyObject *self, PyObject *args)
{
    ThreadObject *this = (ThreadObject *)self;
    return Py_BuildValue("i", this->thread->number);
}

PyObject *p_btp_thread_set_number(PyObject *self, PyObject *args)
{
    ThreadObject *this = (ThreadObject *)self;
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Thread number must not be negative.");
        return NULL;
    }

    this->thread->number = newvalue;
    Py_RETURN_NONE;
}

/* methods */
PyObject *p_btp_thread_dup(PyObject *self, PyObject *args)
{
    ThreadObject *this = (ThreadObject *)self;
    if (thread_prepare_linked_list(this) < 0)
        return NULL;

    ThreadObject *to = (ThreadObject *)PyObject_MALLOC(sizeof(ThreadObject));
    if (!to)
        return PyErr_NoMemory();
    PyObject_INIT(to, &ThreadTypeObject);

    to->thread = btp_thread_dup(this->thread, false);
    if (!to->thread)
        return NULL;

    to->frames = thread_prepare_frame_list(to->thread);

    return (PyObject *)to;
}

PyObject *p_btp_thread_cmp(PyObject *self, PyObject *args)
{
    ThreadObject *this = (ThreadObject *)self;
    PyObject *compare_to;
    if (!PyArg_ParseTuple(args, "O!", &ThreadTypeObject, &compare_to))
        return NULL;

    ThreadObject *cmp_to = (ThreadObject *)compare_to;

    if (thread_prepare_linked_list(this) < 0)
        return NULL;
    if (thread_prepare_linked_list(cmp_to) < 0)
        return NULL;

    return Py_BuildValue("i", btp_thread_cmp(this->thread, cmp_to->thread));
}

PyObject *p_btp_thread_quality_counts(PyObject *self, PyObject *args)
{
    ThreadObject *this = (ThreadObject *)self;
    if (thread_prepare_linked_list(this) < 0)
        return NULL;

    int ok = 0, all = 0;
    btp_thread_quality_counts(this->thread, &ok, &all);
    return Py_BuildValue("(ii)", ok, all);
}

PyObject *p_btp_thread_quality(PyObject *self, PyObject *args)
{
    ThreadObject *this = (ThreadObject *)self;
    if (thread_prepare_linked_list(this) < 0)
        return NULL;

    return Py_BuildValue("f", btp_thread_quality(this->thread));
}