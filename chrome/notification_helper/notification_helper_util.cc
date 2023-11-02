// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/notification_helper/notification_helper_util.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"

namespace notification_helper {

base::FilePath GetChromeExePath() {
  // Look for chrome.exe one folder above notification_helper.exe (as expected
  // in Chrome installs). Failing that, look for it alonside
  // notification_helper.exe.
  base::FilePath dir_exe;
  if (!base::PathService::Get(base::DIR_EXE, &dir_exe))
    return base::FilePath();

  base::FilePath chrome_exe =
      dir_exe.DirName().Append(chrome::kBrowserProcessExecutableName);

  if (!base::PathExists(chrome_exe)) {
    chrome_exe = dir_exe.Append(chrome::kBrowserProcessExecutableName);
    if (!base::PathExists(chrome_exe))
      return base::FilePath();
  }
  return chrome_exe;
}

}  // namespace notification_helper
