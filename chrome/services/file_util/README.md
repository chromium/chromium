The FileUtilService is used to perform operation on archives in a sandboxed
utility process. It currently provides the following operations
- Creation of a ZIP archive from a collection of file handles
- Extraction of a single file from a .TAR or .TAR.XZ archive
- Analysis of ZIP, RAR, DMG, and 7Z archives for Safe Browsing

Code in //chrome/services/file_util is expected to run within a restrictive
utility process sandbox. Code in //chrome/services/file_util/public/ can be run
from privileged processes, including the browser process.
