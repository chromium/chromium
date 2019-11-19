// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_INSTALLER_H_
#define CHROME_UPDATER_WIN_INSTALLER_INSTALLER_H_

#include <windows.h>

#include "chrome/updater/win/installer/exit_code.h"
#include "chrome/updater/win/installer/string.h"

namespace updater {

// A container of a process exit code (eventually passed to ExitProcess) and
// a Windows error code for cases where the exit code is non-zero.
struct ProcessExitResult {
  DWORD exit_code;
  DWORD windows_error;

  explicit ProcessExitResult(DWORD exit) : exit_code(exit), windows_error(0) {}
  ProcessExitResult(DWORD exit, DWORD win)
      : exit_code(exit), windows_error(win) {}

  bool IsSuccess() const { return exit_code == SUCCESS_EXIT_CODE; }
};

// A stack-based string large enough to hold an executable to run
// (which is a path), plus a few extra arguments.
using CommandString = StackString<MAX_PATH * 4>;

// Main function for the installer.
ProcessExitResult WMain(HMODULE module);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_INSTALLER_H_
