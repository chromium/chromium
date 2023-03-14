// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WIN_DELAY_LOAD_FAILURE_SUPPORT_H_
#define CHROME_COMMON_WIN_DELAY_LOAD_FAILURE_SUPPORT_H_

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

namespace chrome {

// This function is called from both the delay load failure hooks present in the
// main EXE and the main DLL to perform any additional common processing during
// a delay load failure event.
FARPROC WINAPI HandleDelayLoadFailureCommon(unsigned reason,
                                            DelayLoadInfo* dll_info);

}  // namespace chrome

#endif  // CHROME_COMMON_WIN_DELAY_LOAD_FAILURE_SUPPORT_H_
