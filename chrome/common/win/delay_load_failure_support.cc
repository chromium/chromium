// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/process/memory.h"

namespace chrome {

FARPROC WINAPI HandleDelayLoadFailureCommon(unsigned reason,
                                            DelayLoadInfo* dll_info) {
  // ERROR_COMMITMENT_LIMIT means that there is no memory. Convert this into a
  // more suitable crash rather than just CHECKing in this function.
  if (dll_info->dwLastError == ERROR_COMMITMENT_LIMIT) {
    base::TerminateBecauseOutOfMemory(0);
  }

  DEBUG_ALIAS_FOR_CSTR(dll_name, dll_info->szDll, 256);
  SCOPED_CRASH_KEY_STRING256("DelayLoad", "ModuleName", dll_name);

  // Deterministically crash here. Returning 0 from the hook would likely result
  // in the process crashing anyway, but in a form that might trigger undefined
  // behavior or be hard to diagnose. See https://crbug.com/1320845.
  CHECK(false);

  return 0;
}

}  // namespace chrome
