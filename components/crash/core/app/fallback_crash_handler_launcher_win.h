// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLER_LAUNCHER_WIN_H_
#define COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLER_LAUNCHER_WIN_H_

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/win/scoped_handle.h"
#include "base/win/startup_information.h"

namespace crash_reporter {

// This class is a last-ditch crash handler for the Crashpad handler process.
// It prepares and stores a command line, ready for dispatching when
// an exception occurs. Everything needed to call CreateProcess is pre-allocated
// to minimize the odds of re-faulting due to e.g. tripping over the same issue
// that caused the initial crash.
// This is still very much best-effort, as doing anything at all inside a
// process that's crashed is always going to be iffy^2.
class FallbackCrashHandlerLauncher {
 public:
  FallbackCrashHandlerLauncher();

  FallbackCrashHandlerLauncher(const FallbackCrashHandlerLauncher&) = delete;
  FallbackCrashHandlerLauncher& operator=(const FallbackCrashHandlerLauncher&) =
      delete;

  ~FallbackCrashHandlerLauncher();

  // Initializes everything that's needed in LaunchAndWaitForHandler.
  bool Initialize(const base::CommandLine& program,
                  const base::FilePath& crashpad_database);

  // Launch the pre-computed command line for the fallback error handler.
  // The expectation is that this function will never return, as the fallback
  // error handler should terminate it with the exception code as the process
  // exit code. The return value from this function is therefore academic in
  // the normal case.
  // However, for completeness, this function returns one of:
  // - The error from CreateProcess if it fails to launch the fallback handler.
  // - The error from waiting on the fallback crash handler process if it
  //   fails to wait for that process to exit.
  // - The exit code from the fallback crash handler process.
  // - The error encountered in retrieving the crash handler process' exit code.
  // Note that the return value is used in testing.
  DWORD LaunchAndWaitForHandler(EXCEPTION_POINTERS* pointers);

 private:
  // A copy of the actual exception pointers made at time of exception.
  EXCEPTION_POINTERS exception_pointers_;

  // The precomputed startup info and command line for launching the fallback
  // handler.
  base::win::StartupInformation startup_info_;
  // Stores the pre-cooked command line, with an allotment of zeros at the back
  // sufficient for writing in the thread id, just before launch.
  std::vector<wchar_t> cmd_line_;

  // An inheritable handle to our own process, the raw handle is necessary
  // for pre-computing the startup info.
  base::win::ScopedHandle self_process_handle_;
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLER_LAUNCHER_WIN_H_
