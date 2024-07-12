// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <wchar.h>

#include <string>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/win/installer/installer.h"
#include "chrome/updater/win/ui/l10n_util.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

int WINAPI wWinMain(HINSTANCE /* instance */,
                    HINSTANCE /* previous_instance */,
                    LPWSTR command_line,
                    int /* command_show */) {
  base::AtExitManager exit_manager;

  updater::ProcessExitResult result =
      updater::WMain(reinterpret_cast<HMODULE>(&__ImageBase));

  // If errors occur, display UI only for metainstaller errors, and also only
  // when the metainstaller runs without command line arguments.
  if (result.exit_code != updater::SUCCESS_EXIT_CODE &&
      wcslen(command_line) == 0) {
    const std::wstring metainstaller_error_string =
        updater::GetLocalizedMetainstallerErrorString(result.exit_code,
                                                      result.windows_error);
    if (!metainstaller_error_string.empty()) {
      base::FilePath exe_path;
      base::PathService::Get(base::FILE_EXE, &exe_path);
      ::MessageBoxEx(nullptr, metainstaller_error_string.c_str(),
                     exe_path.BaseName().value().c_str(), 0, 0);
    }
  }
  return result.exit_code;
}
