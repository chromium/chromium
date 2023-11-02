// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_H_
#define CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_H_

#include <windows.h>

#include <string>
#include <vector>

#include "chrome/chrome_cleaner/os/scoped_service_handle.h"

namespace chrome_cleaner {

// Based on ENUM_SERVICE_STATUS_PROCESSW from
// https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/ns-winsvc-_enum_service_status_processw
struct ServiceStatus {
  std::wstring service_name;
  std::wstring display_name;
  SERVICE_STATUS_PROCESS service_status_process;
};

// Set the current process to background mode.
void SetBackgroundMode();

// Return whether Windows is performing registry key redirection for Windows
// on Windows for the current process.
// Do not use this value as sign of x64 architecture, as it true only for
// 32-bit processes running on 64-bit OS.
bool IsWowRedirectionActive();

// Return whether this version of Windows uses x64 processor architecture.
bool IsX64Architecture();

// Return whether the current process is 64-bit.
bool IsX64Process();

// Fills |services| with services from the given |service_manager| that match
// |service_type| and |service_state|. Returns false on error. The possible
// values of |service_type| and |service_state| are given at
// https://docs.microsoft.com/en-us/windows/desktop/api/winsvc/nf-winsvc-enumservicesstatusexa.
bool EnumerateServices(const ScopedScHandle& service_manager,
                       DWORD service_type,
                       DWORD service_state,
                       std::vector<ServiceStatus>* services);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_H_
