# Allocation Recorder
The allocation_recorder lets Crashpad include a log of recent calls to malloc()
and free() when a memory safety fault is detected. Each entry includes a portion
of the stack trace as well as the size and the address of the object. The code
that generates the log lives in base/debug/allocation_trace.h

## Code organization
The component is split up into multiple parts.
 - **crash_client**: To be used by the client, that is the process for which
   a crash report shall be created. The client also contains the definition of
   the recorder.
 - **crash_handler**: To be used by the crashpad handler. In case of a crash, the
   crash handler is responsible to create a proper report from the recorder data.
   *This will be implemented in a follow up CL.*
 - **internal**: Code that is shared between crash_client and crash_handler but
   used only internally, i.e. name of the crashpad annotation used to store
   data.

## Current state of implementation
Due to the size of implementation, the feature will be implemented in multiple
CLs.

| Nr | CL | Description |
| -- | -- | ----------- |
| 1  | https://chromium-review.googlesource.com/c/chromium/src/+/4323818 | **crash_client**:  Introduce the crash client. The client should be fully implemented afterwards. |
