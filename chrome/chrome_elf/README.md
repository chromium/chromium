# Chrome Early Loading Framework (ELF)

The Chrome Early Loading Framework provides a facility to execute code very
early during process startup before other imported DLLs (e.g., `kernel32`). This
is useful for dynamic patching of system functions (e.g., `NtMapViewOfSection`)
to implement advanced control over the process.

Chrome's initial entry point on Windows is
[DllMain](https://docs.microsoft.com/en-us/windows/desktop/dlls/dllmain) in
`chrome_elf.dll` (defined in [chrome_elf_main.cc](./chrome_elf_main.cc)). The
[reorder-imports.py](../../build/win/reorder-imports.py) script is used at
build-time to ensure that `chrome_elf.dll` is the first import of `chrome.exe`.
