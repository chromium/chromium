// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

#include <memory>

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

// Verify that |kChromeVersion| is equal to the version in the VS_VERSION_INFO
// resource of chrome.exe.
TEST(ChromeConstants, ChromeVersion) {
  base::FilePath current_exe_dir;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &current_exe_dir));
  base::FilePath chrome_exe_path =
      current_exe_dir.Append(chrome::kBrowserProcessExecutableName);

  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfo(chrome_exe_path));
  ASSERT_TRUE(file_version_info);
  EXPECT_EQ(base::UTF16ToASCII(file_version_info->file_version()),
            kChromeVersion);
}

}  // namespace chrome
