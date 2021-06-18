The PDF component contains code necessary for using the PDF plugin in
content-based clients. It provides an implementation for the PPB\_PDF PPAPI
interface, and the necessary browser and renderer-side code for processing the
relevant IPC messages. The PDF plugin code lives in `//pdf`.

TODO(crbug.com/702993): Update description when the PDF plugin no longer uses
PPAPI and is also no longer a plugin process.
