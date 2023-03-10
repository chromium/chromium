// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/strings/string_util.h"
#include "chrome/common/win/delay_load_failure_support.h"

namespace chrome {

namespace {

// Delay load failure hook that generates a crash report. By default a failure
// to delay load will trigger an exception handled by the delay load runtime and
// this won't generate a crash report.
FARPROC WINAPI DelayLoadFailureHook(unsigned reason, DelayLoadInfo* dll_info) {
  char dll_name[MAX_PATH];
  base::strlcpy(dll_name, dll_info->szDll, std::size(dll_name));
  // It's not an error if "bthprops.cpl" fails to be loaded, there's a custom
  // exception handler in 'device/bluetooth/bluetooth_init_win.cc" that will
  // intercept the exception triggered by the delay load runtime. Returning 0
  // will tell the runtime that this failure hasn't been handled and it'll cause
  // the exception to be raised.
  if (base::CompareCaseInsensitiveASCII(dll_name, "bthprops.cpl") == 0)
    return 0;

  return HandleDelayLoadFailureCommon(reason, dll_info);
}

}  // namespace

}  // namespace chrome

// Set the delay load failure hook to the function above.
//
// The |__pfnDliFailureHook2| failure notification hook gets called
// automatically by the delay load runtime in case of failure, see
// https://docs.microsoft.com/en-us/cpp/build/reference/failure-hooks?view=vs-2019
// for more information about this.
extern "C" const PfnDliHook __pfnDliFailureHook2 = chrome::DelayLoadFailureHook;
