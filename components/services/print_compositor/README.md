The print_compositor service should composite multiple raw pictures from
different frames into a complete one, then converts it into a print document
format, either PDF or XPS.  This all happens within an isolated sandboxed
process.

While the compositor creates single-page PDF objects it can optionally collect
those into a multi-page PDF or XPS document object.  Otherwise a multi-page PDF
document is made by sending an extra multi-page metafile which contains repeats
of each of the previously processed pages all in one larger message.

Message flow when interacting with the print document compositor is as follows:

[![IPC flow for print compositor
usage](ipc_flow_diagram.png)](https://docs.google.com/drawings/d/1bhm3FfLaSL42f-zw41twnOGG0kdMKMuAGoEyGuGr6HQ)