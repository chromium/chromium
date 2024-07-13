// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <wchar.h>

#include <string>

#include "base/at_exit.h"
#include "chrome/updater/win/installer/installer.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

int WINAPI wWinMain(HINSTANCE /* instance */,
                    HINSTANCE /* previous_instance */,
                    LPWSTR command_line,
                    int /* command_show */) {
  base::AtExitManager exit_manager;
  return updater::WMain(reinterpret_cast<HMODULE>(&__ImageBase));
}
