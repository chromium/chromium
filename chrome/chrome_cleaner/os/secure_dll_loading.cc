// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/logging.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/os/secure_dll_loading.h"

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
#include "base/command_line.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#endif  // CHROME_CLEANER_OFFICIAL_BUILD

namespace chrome_cleaner {

const wchar_t kEmptyDll[] = L"empty_dll.dll";

bool EnableSecureDllLoading() {
#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  // We can't use base::CommandLine::ForCurrentProcess() here because that
  // initializes CommandLine, which breaks expectations later about CommandLine
  // being unset.
  base::CommandLine command_line =
      base::CommandLine::FromString(::GetCommandLineW());
  if (command_line.HasSwitch(chrome_cleaner::kAllowUnsecureDLLsSwitch))
    return false;
#endif

  typedef BOOL(WINAPI * SetDefaultDllDirectoriesFunction)(DWORD flags);
  SetDefaultDllDirectoriesFunction set_default_dll_directories =
      reinterpret_cast<SetDefaultDllDirectoriesFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "SetDefaultDllDirectories"));
  if (!set_default_dll_directories) {
    // Don't assert because this is known to be missing on Windows 7 without
    // KB2533623.
    LOG(WARNING) << "SetDefaultDllDirectories unavailable";
    return false;
  }

  if (!set_default_dll_directories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
    NOTREACHED() << "Encountered error calling SetDefaultDllDirectories";
    return false;
  }

  return true;
}

namespace testing {

void LoadEmptyDLL() {
  ::LoadLibraryW(kEmptyDll);
}

}  // namespace testing

}  // namespace chrome_cleaner
