/*
 * Based on the excellent example by adamlamers (http://adamlamers.com/post/NUBSPFQJ50J1)
 */
#include <Python.h>
#include <stdio.h>
#include <unistd.h>

#include <sundtek/mcsimple.h>
#include <sundtek/mediaclient.h>

#include "sundtek_api.h"

// The following definitions are needed for sundtek api calls:
#define ALLOCATE_DEVICE_OBJECT 1   // allocate device objects when running media_scan_network
#define NETWORK_SCAN_TIME 700      // Why 700 ms? It seems to work...

// for marking local devices and mounted network devices
#define LOCAL_DEVICE 0
#define NETWORK_DEVICE 2
#define MOUNTED_NETWORK_DEVICE 4

// define module constants
const char *NAME_IR_PROTO_NEC        = "IR_PROTO_NEC";
const char *NAME_IR_PROTO_RC5        = "IR_PROTO_RC5";
const char *NAME_IR_PROTO_RC6_MODE0  = "IR_PROTO_RC6_MODE0"; 
const char *NAME_IR_PROTO_RC6_MODE6A = "IR_PROTO_RC6_MODE6A";

// Actual module method definition - this is the code that will be called by
// sundtek.local_devices() in python
static PyObject *sundtek_api_local_devices(PyObject *self, PyObject *args)
{
   int fd = connect_sundtek_mediasrv();
   if (fd <= 0) {
      PyErr_SetString(PyExc_ConnectionError, "connecting to mediasrv failed");
      return (PyObject *) NULL;
   }

   PyObject *local_devices = PyDict_New();
   local_device_scan(fd, local_devices);

   net_close(fd); // close the connection to the local mediasrv

   return local_devices;
}

// Actual module method definition - this is the code that will be called by
// sundtek.network_devices() in python
static PyObject *sundtek_api_network_devices(PyObject *self, PyObject *args) {

   // scan for all devices
   void *fd = media_scan_network(ALLOCATE_DEVICE_OBJECT, NETWORK_SCAN_TIME);
   PyObject *network_devices = PyDict_New();
   network_device_scan(fd, network_devices);
   media_scan_free(&fd);
   return network_devices;
}

static PyObject *sundtek_api_enable_network(PyObject *self, PyObject *args) {
   int fd = connect_sundtek_mediasrv();
   if (fd <= 0) {
      PyErr_SetString(PyExc_ConnectionError, "connecting to mediasrv failed");
      return (PyObject *) NULL;
   }

   net_enablenetwork(1);
   net_close(fd);
   Py_RETURN_NONE;
}

static PyObject *sundtek_api_disable_network(PyObject *self, PyObject *args) {
   int fd = connect_sundtek_mediasrv();
   if (fd <= 0) {
      PyErr_SetString(PyExc_ConnectionError, "connecting to mediasrv failed");
      return (PyObject *) NULL;
   }

   net_enablenetwork(0);
   net_close(fd);
   Py_RETURN_NONE;
}

static PyObject *sundtek_api_mount(PyObject *self, PyObject *args) {
   const char *path;
   if (!PyArg_ParseTuple(args, "s", &path))
       return NULL;

   int fd = connect_sundtek_mediasrv();
   if (fd <= 0) {
      PyErr_SetString(PyExc_ConnectionError, "connecting to mediasrv failed");
      return (PyObject *) NULL;
   }

   int result = net_mount_device((uint8_t *)path, MEDIA_MOUNT);
   if (result != 0) {
      PyErr_SetString(PyExc_ConnectionError, "mounting device failed");
      return (PyObject *) NULL;
   }
   net_close(fd);
   Py_RETURN_NONE;
}

static PyObject *sundtek_api_umount(PyObject *self, PyObject *args) {
   const char *path;
   if (!PyArg_ParseTuple(args, "s", &path))
       return NULL;

   int fd = connect_sundtek_mediasrv();
   if (fd <= 0) {
      PyErr_SetString(PyExc_ConnectionError, "connecting to mediasrv failed");
      return (PyObject *) NULL;
   }

   int result = net_mount_device((uint8_t*)path, MEDIA_UNMOUNT);
   if (result != 0) {
      PyErr_SetString(PyExc_ConnectionError, "umounting device failed");
      return (PyObject *) NULL;
   }
   net_close(fd);
   Py_RETURN_NONE;
}

static PyObject *sundtek_get_ir_protocols(PyObject *self, PyObject *args) {
   char *path;
   if (!PyArg_ParseTuple(args, "s", &path))
       return NULL;

   PyObject *ir_settings = PyDict_New();
   PyDict_SetItemString(ir_settings, "active_ir_protocol", Py_BuildValue("i", 0));

   PyObject *ir_protocols = PyDict_New();
   PyDict_SetItem(ir_protocols, PyLong_FromLong(IR_PROTO_NEC), Py_BuildValue("s", "NEC (hardware decoder)"));

   int dev_fd = net_open(path, O_RDWR);
   if (dev_fd >= 0) {
      struct media_ir_enum ir;
      int8_t ret = 0;
      memset(&ir, 0x0, sizeof(struct media_ir_enum));

      while((ret=net_ioctl(dev_fd, DEVICE_ENUM_IR, &ir)) == 0) {
         if (ir.active) PyDict_SetItemString(ir_settings, "active_ir_protocol", Py_BuildValue("i", ir.id));
         PyDict_SetItem(ir_protocols, PyLong_FromLong(ir.id), Py_BuildValue("s", (char*)ir.name));
         ir.id++;
      }
      net_close(dev_fd);
   }
   PyDict_SetItemString(ir_settings, "ir_protocols", ir_protocols);
   return ir_settings;
}

static PyObject *sundtek_set_ir_protocol(PyObject *self, PyObject *args) {
   char *path;
   int  protocol;
   if (!PyArg_ParseTuple(args, "si", &path, &protocol))
       return NULL;
   int dev_fd = net_open(path, O_RDWR);
   if (dev_fd >= 0) {
       int resp = set_ir_protocol(protocol, path);
       if (!resp) {
           PyErr_SetString(PyExc_ValueError, "Could not set ir protocol");
           return NULL;
       }
   }
   net_close(dev_fd);
   Py_RETURN_NONE;
}

//Method definition object for this extension, these argumens mean:
//ml_name: The name of the method
//ml_meth: Function pointer to the method implementation
//ml_flags: Flags indicating special features of this method, such as
//          accepting arguments, accepting keyword arguments, being a
//          class method, or being a static method of a class.
//ml_doc:  Contents of this method's docstring
static PyMethodDef sundtek_api_methods[] = {
    {
	"local_devices",
	sundtek_api_local_devices,
	METH_NOARGS,
	"returns available sundtek devices (either connected locally or mounted) using the mcsimple api."
    },
    {   "network_devices",
	sundtek_api_network_devices,
	METH_NOARGS,
	"returns all network devices announced by other mediasrv instances"
    },
    {
        "enable_network",
	sundtek_api_enable_network,
	METH_NOARGS,
	"enable network sharing of local sundtek devices"
    },
    {
        "disable_network",
	sundtek_api_disable_network,
	METH_NOARGS,
	"disable network sharing of local sundtek devices"
    },
    {
        "mount",
	sundtek_api_mount,
	METH_VARARGS,
	"mount a network device"
    },
    {
        "umount",
	sundtek_api_umount,
	METH_VARARGS,
	"umount a network device"
    },
    {
        "ir_protocols",
        sundtek_get_ir_protocols,
        METH_VARARGS,
        "return ir protocols settings for a frontend node"
    },
    {
        "set_ir_protocol",
	sundtek_set_ir_protocol,
	METH_VARARGS,
	"set ir protocol settings for a frontend node.\n\nThis may fail without raising an error."
    },
    /*
    {   "is_local_device",
	sundtek_api_is_local_device,
	METH_VARARGS,
	"accepts either a device id (int) or a serial (str), returns True if a matching device is physically connected."
    },
    */
    {NULL, NULL, 0, NULL}
};

//Module definition
//The arguments of this structure tell Python what to call your extension,
//what it's methods are and where to look for it's method definitions
static struct PyModuleDef sundtek_api_definition = {
    PyModuleDef_HEAD_INIT,
    "sundtek_api",
    "A Python module that calls the sundtek API from C code.",
    -1,
    sundtek_api_methods
};

//Module initialization
//Python calls this function when importing your extension. It is important
//that this function is named PyInit_[[your_module_name]] exactly, and matches
//the name keyword argument in setup.py's setup() call.
PyMODINIT_FUNC PyInit_sundtek_api(void)
{
    Py_Initialize();

    PyObject *m = NULL;
    m = PyModule_Create(&sundtek_api_definition);
    if (m == NULL) {
       goto except;
    }

    if (PyModule_AddIntConstant(m, NAME_IR_PROTO_NEC, IR_PROTO_NEC)) {
       goto except;
    }
    if (PyModule_AddIntConstant(m, NAME_IR_PROTO_RC5, IR_PROTO_RC5)) {
       goto except;
    }
    if (PyModule_AddIntConstant(m, NAME_IR_PROTO_RC6_MODE0, IR_PROTO_RC6_MODE0)) {
       goto except;
    }
    if (PyModule_AddIntConstant(m, NAME_IR_PROTO_RC6_MODE6A, IR_PROTO_RC6_MODE6A)) {
       goto except;
    }
    goto finally;
except:
    Py_XDECREF(m);
    m = NULL;
finally:
    return m;
}

// helper functions (actual implementation of the c api calls)

int connect_sundtek_mediasrv(void) {
   /*
    * obtains a connection to the sundtek mediasrv and returns a file desriptor
    * if the connections fails fd is <= 0. You must close the connection
    * by calling net_close(fd)
    */

   // the sundtek libraries write error messages to stdout
   // so we redirect it to stderr...
   int old_stdout = dup(STDOUT_FILENO);
   dup2(STDERR_FILENO, STDOUT_FILENO);

   // obtain a connection to mediasrv
   int fd = net_connect(0);

   // switch back to "normal" stdout for dumping our JSON data after flushing remaining data
   fflush(stdout);
   dup2(old_stdout, STDOUT_FILENO);

   return fd;
}

int device_type(const char *serial) {
   struct media_device_enum *device;
   int device_index = 0;
   int subdevice;
   int fd = connect_sundtek_mediasrv();
   int device_type = NETWORK_DEVICE;
   if (fd <= 0) { // if the local mediasrv is not reachable it can't be a local device
      return device_type;
   }
   while (1) {
      subdevice = 0;
      while (1) {
         device = net_device_enum(fd, &device_index, subdevice);
         if (!device)
            break;
         if (strcmp((const char*)device->serial, serial) == 0) {
             if ((device->capabilities & MEDIA_REMOTE_DEVICE) > 0) {
                device_type = MOUNTED_NETWORK_DEVICE;
                goto close;
             } else {
                device_type = LOCAL_DEVICE;
                goto close;
             }
         }         subdevice++;
         free(device);
         device = NULL;
      }
      if (subdevice == 0)
         break;
      device_index++;
   }
   close:
      if (device)
         free(device);
   net_close(fd);
   return device_type;
}

void local_device_scan(int fd, PyObject *local_devices) {
   /*
    * use a given fd to communicate with mediasrv.
    * iterate over all local devices,
    * call device2dict to add their data to the dictionary* local_devices passed to the function.
    */

   struct media_device_enum *device;
   int device_index = 0;
   int subdevice = 0;
   while((device=net_device_enum(fd, &device_index, subdevice))!=0) {   // multi frontend support???
      do {
	 device2dict(device, local_devices);
	 free(device);
	 device = NULL;
      } while((device=net_device_enum(fd, &device_index, ++subdevice))!=0);
      subdevice = 0;
      device_index++;
   }
}

void network_device_scan(void *fd, PyObject *network_devices) {
   /*
    * takes a fd returned my media_scan_network(), fills network_devices with
    * devices announced by non-local mediasrv instances.
    */
   char *id = NULL;
   char *ip = NULL;
   char *name = NULL;
   char *serial = NULL;
   uint32_t *cap = NULL;
   uint32_t net_cap;

   int n = 0;
   while (media_scan_info(fd, n, "ip", (void**) &ip) == 0) {
      media_scan_info(fd, n, "serial", (void**) &serial);
      int device_type_result = device_type(serial);
      if (device_type_result > LOCAL_DEVICE) {
         media_scan_info(fd, n, "capabilities", (void**) &cap);
         media_scan_info(fd, n, "devicename",   (void**) &name);
         media_scan_info(fd, n, "id",           (void**) &id);
    
         PyObject *capabilities = PyDict_New();
    
         net_cap = *cap | (uint32_t) MEDIA_REMOTE_DEVICE; // mark all devices as network devices
         capabilities2dict(net_cap, capabilities);
    
         PyObject *sundtek_device = PyDict_New();
         PyDict_SetItemString(sundtek_device, "capabilities", capabilities);
         PyDict_SetItemString(sundtek_device, "devicename",   Py_BuildValue("s", name));
         PyDict_SetItemString(sundtek_device, "id",           Py_BuildValue("i", (atoi(id))));
         PyDict_SetItemString(sundtek_device, "ip",           Py_BuildValue("s", ip));
         PyDict_SetItemString(sundtek_device, "serial",       Py_BuildValue("s", serial));
         PyDict_SetItemString(sundtek_device, "mounted",      PyBool_FromLong((device_type_result == MOUNTED_NETWORK_DEVICE)));

         PyDict_SetItemString(network_devices, serial, sundtek_device);
      }
      n++;
   }
}

void capabilities2dict(const uint32_t cap, PyObject *capabilities) {
   PyDict_SetItemString(capabilities, "analog_tv",        PyBool_FromLong(cap & MEDIA_ANALOG_TV));
   PyDict_SetItemString(capabilities, "atsc",             PyBool_FromLong(cap & MEDIA_ATSC));
   PyDict_SetItemString(capabilities, "bitmask",          PyLong_FromUnsignedLong(cap)); // raw bitmask
   PyDict_SetItemString(capabilities, "digitalca",        PyBool_FromLong(cap & MEDIA_DIGITAL_CA));
   PyDict_SetItemString(capabilities, "digitalci",        PyBool_FromLong(cap & MEDIA_DIGITAL_CI));
   PyDict_SetItemString(capabilities, "dvbc",             PyBool_FromLong(cap & MEDIA_DVBC));
   PyDict_SetItemString(capabilities, "dvbc2",            PyBool_FromLong(cap & MEDIA_DVBC2));
   PyDict_SetItemString(capabilities, "dvbs2",            PyBool_FromLong(cap & MEDIA_DVBS2));
   PyDict_SetItemString(capabilities, "dvbt",             PyBool_FromLong(cap & MEDIA_DVBT));
   PyDict_SetItemString(capabilities, "dvbt2",            PyBool_FromLong(cap & MEDIA_DVBT2));
   PyObject *initial_dvb_mode = PyDict_New();
   if ((cap & MEDIA_DIGITAL_TV) > 0) {
    PyDict_SetItemString(initial_dvb_mode, "DVB-C", Py_BuildValue("s", "DVBC"));
    PyDict_SetItemString(initial_dvb_mode, "DVB-T", Py_BuildValue("s", "DVBT"));
   }
   PyDict_SetItemString(capabilities, "initial_dvb_mode", initial_dvb_mode);
   PyDict_SetItemString(capabilities, "network_device",   PyBool_FromLong(cap & MEDIA_REMOTE_DEVICE));
   PyDict_SetItemString(capabilities, "radio",            PyBool_FromLong(cap & MEDIA_RADIO));
   PyDict_SetItemString(capabilities, "remote",           PyBool_FromLong(cap & MEDIA_REMOTE));
}

void device2dict(struct media_device_enum *device, PyObject *local_devices) {
   /*
    * Take a media_device_enum and add the data transformed into python dicts
    * to the local_devices object if it is a local device
    */

   if ((device->capabilities & MEDIA_REMOTE_DEVICE) > 0) {
      return;
   }

   PyObject *capabilities = PyDict_New();
   capabilities2dict(device->capabilities, capabilities);

   PyObject *sundtek_device = PyDict_New();
   PyDict_SetItemString(sundtek_device, "capabilities",  capabilities);
   PyDict_SetItemString(sundtek_device, "devicename",    Py_BuildValue("s", (char*)device->devicename));
   PyDict_SetItemString(sundtek_device, "frontend_node", Py_BuildValue("s", (char*)device->frontend_node));
   PyDict_SetItemString(sundtek_device, "id",            Py_BuildValue("i", device->id));
   PyDict_SetItemString(sundtek_device, "remote_node",   Py_BuildValue("s", (char*)device->remote_node));
   PyDict_SetItemString(sundtek_device, "serial",        Py_BuildValue("s", (char*)device->serial));


   PyDict_SetItemString(local_devices, (char*)device->serial, sundtek_device);
}

int set_ir_protocol(int ir_protocol, char *frontend_node) {
   /*
    * takes the ir protocol number and the frontend node
    * to set the ir protocol (this can fail without error,
    * if the device does not support setting a protocol).
    */


   int fd = net_open(frontend_node, O_RDWR);
   if (fd > 0) {
      struct media_ir_config ir_config;
      ir_config.protocol = ir_protocol;
      ir_config.nec_parity = 0;
      ir_config.rc6_mode = 0;
      int response = net_ioctl(fd, DEVICE_CONFIG_IR, &ir_config);
      net_close(fd);
      return (response == 0);
   }
   return 0;
}
