// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_PROCESS_H_
#define CHROME_CHROME_CLEANER_OS_PROCESS_H_

#include <windows.h>

#include <set>
#include <string>

#include "base/process/process_metrics_iocounters.h"
#include "base/time/time.h"

namespace chrome_cleaner {

// Contains system resource usage information of a process.
struct SystemResourceUsage {
  base::IoCounters io_counters;
  base::TimeDelta user_time;
  base::TimeDelta kernel_time;
  size_t peak_working_set_size;  // In bytes.
};

// This returns a string instead of a base::FilePath because it is called from
// SandboxGetLoadedModules, which needs to handle invalid UTF-16 characters
// gracefully. (Technically Windows file paths can contain arbitrary 16-bit
// values that may not be valid UTF-16.)
// Provided handle should have PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
// access rights.
// The function might not work when enumerating modules of x64 process from a
// x86 process.
bool GetLoadedModuleFileNames(HANDLE process,
                              std::set<std::wstring>* module_names);

// Retrieve process executable module in win32 path format.
// Provided handle must have PROCESS_QUERY_LIMITED_INFORMATION or
// PROCESS_QUERY_INFORMATION access right.
bool GetProcessExecutablePath(HANDLE process, std::wstring* path);

// Retrieves system resource usage stats for the given process.
// Provided handle must have PROCESS_QUERY_LIMITED_INFORMATION or
// PROCESS_QUERY_INFORMATION access right.
bool GetSystemResourceUsage(HANDLE process, SystemResourceUsage* stats);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_PROCESS_H_
