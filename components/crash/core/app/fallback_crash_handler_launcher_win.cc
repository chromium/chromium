// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/fallback_crash_handler_launcher_win.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/win_util.h"

namespace crash_reporter {

namespace {

// The number of characters reserved at the tail of the command line for the
// thread ID parameter.
const size_t kCommandLineTailSize = 32;

}  // namespace

FallbackCrashHandlerLauncher::FallbackCrashHandlerLauncher() {
  memset(&exception_pointers_, 0, sizeof(exception_pointers_));
}

FallbackCrashHandlerLauncher::~FallbackCrashHandlerLauncher() {}

bool FallbackCrashHandlerLauncher::Initialize(
    const base::CommandLine& program,
    const base::FilePath& crashpad_database) {
  // Open an inheritable handle to self. This will be inherited to the handler.
  const DWORD kAccessMask = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                            PROCESS_DUP_HANDLE | PROCESS_TERMINATE;
  self_process_handle_.Set(
      OpenProcess(kAccessMask, TRUE, ::GetCurrentProcessId()));
  if (!self_process_handle_.IsValid())
    return false;

  // Setup the startup info for inheriting the self process handle into the
  // fallback crash handler.
  if (!startup_info_.InitializeProcThreadAttributeList(1))
    return false;

  HANDLE raw_self_process_handle = self_process_handle_.Get();
  if (!startup_info_.UpdateProcThreadAttribute(
          PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &raw_self_process_handle,
          sizeof(raw_self_process_handle))) {
    return false;
  }

  // Build the command line from a copy of the command line passed in.
  base::CommandLine cmd_line(program);
  cmd_line.AppendSwitchPath("database", crashpad_database);
  cmd_line.AppendSwitchASCII(
      "exception-pointers",
      base::NumberToString(reinterpret_cast<uintptr_t>(&exception_pointers_)));
  cmd_line.AppendSwitchASCII(
      "process", base::NumberToString(
                     base::win::HandleToUint32(self_process_handle_.Get())));

  std::wstring str_cmd_line = cmd_line.GetCommandLineString();

  // Append the - for now abortive - thread argument manually.
  str_cmd_line.append(L" --thread=");
  // Store the command line string for easy use later.
  cmd_line_.assign(str_cmd_line.begin(), str_cmd_line.end());

  // Resize the vector to reserve space for the thread ID.
  cmd_line_.resize(cmd_line_.size() + kCommandLineTailSize, '\0');

  return true;
}

DWORD FallbackCrashHandlerLauncher::LaunchAndWaitForHandler(
    EXCEPTION_POINTERS* exception_pointers) {
  DCHECK(!cmd_line_.empty());
  DCHECK_EQ('=', cmd_line_[cmd_line_.size() - kCommandLineTailSize - 1]);
  // This program has crashed. Try and not use anything but the stack.

  // Append the current thread's ID to the command line in-place.
  int chars_appended = wsprintf(&cmd_line_.back() - kCommandLineTailSize + 1,
                                L"%d", GetCurrentThreadId());
  DCHECK_GT(static_cast<int>(kCommandLineTailSize), chars_appended);

  // Copy the exception pointers to our member variable, whose address is
  // already baked into the command line.
  exception_pointers_ = *exception_pointers;

  // Launch the pre-cooked command line.

  PROCESS_INFORMATION process_info = {};
  if (!CreateProcess(nullptr,                       // Application name.
                     &cmd_line_[0],                 // Command line.
                     nullptr,                       // Process attributes.
                     nullptr,                       // Thread attributes.
                     true,                          // Inherit handles.
                     0,                             // Creation flags.
                     nullptr,                       // Environment.
                     nullptr,                       // Current directory.
                     startup_info_.startup_info(),  // Startup info.
                     &process_info)) {
    return GetLastError();
  }

  // Wait on the fallback crash handler process. The expectation is that this
  // will never return, as the fallback crash handler will terminate this
  // process. For testing, and full-on belt and suspenders, cover for this
  // returning.
  DWORD error = WaitForSingleObject(process_info.hProcess, INFINITE);
  if (error != WAIT_OBJECT_0) {
    // This should never happen, barring handle abuse.
    // TODO(siggi): Record an UMA metric here.
    NOTREACHED();
    error = GetLastError();
  } else {
    // On successful wait, return the exit code of the fallback crash handler
    // process.
    if (!GetExitCodeProcess(process_info.hProcess, &error)) {
      // This should never happen, barring handle abuse.
      NOTREACHED();
      error = GetLastError();
    }
  }

  // Close the handles returned from CreateProcess.
  CloseHandle(process_info.hProcess);
  CloseHandle(process_info.hThread);

  return error;
}

}  // namespace crash_reporter
