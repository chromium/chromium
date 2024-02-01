// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_
#define CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_

#include <windows.h>

#include "chrome/installer/mini_installer/exit_code.h"
#include "chrome/installer/mini_installer/mini_string.h"
#include "chrome/installer/mini_installer/path_string.h"

namespace mini_installer {

class Configuration;

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
// (which is a path), two additional path arguments, plus a few extra
// arguments. Figure that MAX_PATH (260) is sufficient breathing room for the
// extra arguments.
using CommandString = StackString<MAX_PATH * 4>;

// Populates |path| with the path to the previous version's setup.exe, stripping
// quotes if present.
ProcessExitResult GetPreviousSetupExePath(const Configuration& configuration,
                                          wchar_t* path,
                                          size_t size);

// Populates |directory| with the directory portion of the path to |module|.
// Returns false in case of failure, in which case the contents of |directory|
// are undefined and may have been modified.
bool GetModuleDir(HMODULE module, PathString* directory);

// Appends everything following the path to the executable in |command_line|
// verbatim to |buffer|, including all whitespace, quoted arguments,
// etc. |buffer| is unchanged in case of error.
void AppendCommandLineFlags(const wchar_t* command_line, CommandString* buffer);

// Main function for Chrome's mini_installer. First gets a working dir, unpacks
// the resources, and finally executes setup.exe to do the install/update. Also
// handles invoking a previous version's setup.exe to patch itself in the case
// of differential updates.
ProcessExitResult WMain(HMODULE module);

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_
