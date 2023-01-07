# chrome_wer - Runtime Exception Helper Module for Windows Error Reporting

This builds a DLL which can be registered as a runtime exception helper using
[WerRegisterExceptionHelperModule](https://docs.microsoft.com/en-us/windows/win32/api/werapi/nf-werapi-werregisterruntimeexceptionmodule).

It integrates tightly with crashpad's client (in chrome_elf) and the crashpad
handler process. It should be distributed alongside Chrome and is not intended
to work with a different Chrome version.

Interesting exceptions find their way to the helper starting with Windows 20H1
(19042). Prior versions did not attempt to load the helper.

The full path to the DLL must be registered under:

```
{ HKEY_CURRENT_USER or HKEY_LOCAL_MACHINE }
 \Software
 \Microsoft
 \Windows
 \Windows Error Reporting
 \RuntimeExceptionHelperModules
  Value:{full path to dll} DWORD:{ 0 - any value }
 ```
