// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/auto_launch_util.h"

#include <stdint.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/util_constants.h"
#include "crypto/sha2.h"

namespace {

// The prefix of the Chrome Auto-launch key under the Run key.
const wchar_t kAutolaunchKeyValue[] = L"GoogleChromeAutoLaunch";

// Builds a registry key name to use when deciding where to read/write the auto-
// launch value to/from. It takes into account the path of the profile so that
// different installations of Chrome don't conflict.
std::wstring GetAutoLaunchKeyName() {
  base::FilePath path;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &path))
    NOTREACHED();
  // Background auto-launch is only supported for the Default profile at the
  // moment, but keep the door opened to a multi-profile implementation by
  // encoding the Default profile in the hash.
  path = path.AppendASCII(chrome::kInitialProfile);

  std::string input(path.AsUTF8Unsafe());
  uint8_t hash[16];
  crypto::SHA256HashString(input, hash, std::size(hash));
  return std::wstring(kAutolaunchKeyValue) + L"_" +
         base::ASCIIToWide(base::HexEncode(hash, std::size(hash)));
}

}  // namespace

namespace auto_launch_util {

void EnableBackgroundStartAtLogin() {
  base::FilePath application_dir;
  if (!base::PathService::Get(base::DIR_EXE, &application_dir))
    NOTREACHED();

  base::CommandLine cmd_line(application_dir.Append(installer::kChromeExe));
  cmd_line.AppendSwitch(switches::kNoStartupWindow);
  cmd_line.AppendArg(switches::kPrefetchArgumentBrowserBackground);

  base::win::AddCommandToAutoRun(HKEY_CURRENT_USER, GetAutoLaunchKeyName(),
                                 cmd_line.GetCommandLineString());
}

void DisableBackgroundStartAtLogin() {
  base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER,
                                      GetAutoLaunchKeyName());
}

}  // namespace auto_launch_util
