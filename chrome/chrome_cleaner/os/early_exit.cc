// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/early_exit.h"

#include <windows.h>

#include "base/logging.h"

namespace chrome_cleaner {

void EarlyExit(int exit_code) {
  // Terminate immediately. This terminates more forcefully than _exit(). See
  // http://crbug.com/603131#c27 for more details.
  LOG(ERROR) << "Early exit with code " << exit_code;
  ::TerminateProcess(::GetCurrentProcess(), exit_code);
}

}  // namespace chrome_cleaner
