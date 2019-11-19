# Install Static Library

The install_static library for Windows contains the minimum functionality to
determine the fundamental properties of Chrome's installation plus various
installation-related utility functions. It has no dependencies beyond
kernel32.dll and version.dll, thereby making it suitable for use within
chrome_elf.

Key concepts for the library are documented in
[install_modes.h](./install_modes.h),
[install_constants.h](./install_constants.h), and
[install_details.h](./install_details.h).
