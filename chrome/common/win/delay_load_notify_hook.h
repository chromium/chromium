// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WIN_DELAY_LOAD_NOTIFY_HOOK_H_
#define CHROME_COMMON_WIN_DELAY_LOAD_NOTIFY_HOOK_H_

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

namespace chrome {

// See the following reference for sample delayload hook function:
// https://learn.microsoft.com/en-us/cpp/build/reference/understanding-the-helper-function
// The delayload hook function allows us to modify and monitor the behavior of
// delayload resolution for delay loads from our module. Note that the
// callback function is module-specific and only resolves delayloaded imports
// from the module in which it is set, not imports from other modules and also
// not from calls to LoadLibrary(Ex). In other words, different modules may
// have their own delayload hook with unique delayload resolution logic.
// For example, chrome.exe and chrome.dll have their own global callback
// pointers that are completely separate from each other and have to be
// initialized separately.
//
// |delay_load_event| is |dliNotify| on MSDN and |delay_load_info| is |pdli|.
// Return nullptr (0) for the default behavior. Depending on the event type
// override the behavior by returning an HMODULE or function pointer to
// provide an alternate implementation for a delay load.
typedef FARPROC (*DelayLoadCallbackFunction)(unsigned delay_load_event,
                                             DelayLoadInfo* delay_load_info);

// Set a specific delayload hook at runtime. Note that this callback cannot
// resolve functions that have already been resolved by delayload runtime, so it
// would be practical to set it during early process startup. nullptr can be
// passed to clear the callback, which is used in tests.
void SetDelayLoadHookCallback(DelayLoadCallbackFunction callback_function);

}  // namespace chrome

#endif  // CHROME_COMMON_WIN_DELAY_LOAD_NOTIFY_HOOK_H_
