// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_CLEANER_H_
#define CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_CLEANER_H_

#include <windows.h>

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/win/wrapped_window_proc.h"

namespace base {
class FilePath;
}  // namespace base

namespace chrome_cleaner {

// Adjust process token privileges to acquire debugging privileges, return
// false on failure.
bool AcquireDebugRightsPrivileges();

// Adjust process token privileges to disallow debugging privileges, return
// false on failure.
bool ReleaseDebugRightsPrivileges();

// Returns true if the process currently has debugging privileges.
bool HasDebugRightsPrivileges();

// Verify if the current process has admin rights or not and cache the result.
// @returns true if the current process is running elevated.
bool HasAdminRights();

// Check whether a process is running with the image |executable|. Return true
// if a process is found.
bool IsProcessRunning(const wchar_t* executable);

// Wait until every running instance of |executable| is stopped. Return true if
// every running processes are stopped.
bool WaitForProcessesStopped(const wchar_t* executable);

// Return true when a service with name |service_name| exist.
bool DoesServiceExist(const wchar_t* service_name);

// Wait until service named |service_name| is stopped. Return true if the
// service is stopped or doesn't exist.
bool WaitForServiceStopped(const wchar_t* service_name);

// Wait until service named |service_name| is deleted. Return true if the
// service is deleted.
bool WaitForServiceDeleted(const wchar_t* service_name);

// Check whether a service with name |service_name| exist, and stop it. Return
// true on success.
bool StopService(const wchar_t* service_name);

// Delete the service with name |service_name|. Return true on success.
bool DeleteService(const wchar_t* service_name);

// Call at program startup to configure COM security such that some system APIs
// can be used. Return false on failures.
bool InitializeCOMSecurity();

// Launches an elevated process in a STA-initialize thread and sets the parent
// window to the one specified by |hwnd|. A nullptr can be passed as |hwnd| if
// there is no need to associate the process with a window.
base::Process LaunchElevatedProcessWithAssociatedWindow(
    const base::CommandLine& command_line,
    HWND hwnd);

bool InitializeQuarantineFolder(base::FilePath* output_quarantine_path);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_CLEANER_H_
