// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_INSTALLER_H_
#define CHROME_UPDATER_WIN_INSTALLER_INSTALLER_H_

#include <windows.h>

#include <optional>

#include "base/command_line.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "chrome/updater/win/installer/string.h"

namespace base {
class FilePath;
}  // namespace base

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

inline constexpr size_t kInstallerMaxCommandString = 8191;

// A stack-based string large enough to hold an executable to run
// (which is a path), plus a few extra arguments.
using CommandString = StackString<kInstallerMaxCommandString>;

std::optional<base::FilePath> FindOfflineDir(const base::FilePath& unpack_path);

ProcessExitResult BuildInstallerCommandLineArguments(
    const wchar_t* cmd_line,
    wchar_t* cmd_line_args,
    size_t cmd_line_args_capacity);

// Handles elevating the installer, waiting for the installer process, and
// returning the resulting process exit code.
ProcessExitResult HandleRunElevated(const base::CommandLine& command_line);

// Main function for the installer.
int WMain(HMODULE module);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_INSTALLER_H_
