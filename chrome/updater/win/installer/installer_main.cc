// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wchar.h>
#include <windows.h>

#include <string>

#include "chrome/updater/win/installer/installer.h"
#include "chrome/updater/win/ui/l10n_util.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

int WINAPI wWinMain(HINSTANCE /* instance */,
                    HINSTANCE /* previous_instance */,
                    LPWSTR command_line,
                    int /* command_show */) {
  updater::ProcessExitResult result =
      updater::WMain(reinterpret_cast<HMODULE>(&__ImageBase));

  // If errors occur, display UI only when the metainstaller runs without
  // command line arguments.
  if (result.exit_code != updater::SUCCESS_EXIT_CODE &&
      wcslen(command_line) == 0) {
    ::MessageBoxEx(nullptr,
                   updater::GetLocalizedErrorString(result.exit_code).c_str(),
                   nullptr, 0, 0);
  }
  return result.exit_code;
}
