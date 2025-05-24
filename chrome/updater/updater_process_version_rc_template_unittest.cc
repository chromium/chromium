// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/updater/updater_branding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

struct OriginalFilenameTestCase {
  const std::wstring module_name;
  const std::u16string expected_original_filename;
};

class OriginalFilenameTest
    : public ::testing::TestWithParam<OriginalFilenameTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    OriginalFilenameTestCases,
    OriginalFilenameTest,
    ::testing::ValuesIn(std::vector<OriginalFilenameTestCase>{
        {L"msi_custom_action.dll",
         u"" PRODUCT_FULLNAME_STRING "MsiInstallerCustomAction.dll"},
        {L"qualification_app.exe", u"qualification_app.exe"},
        {L"updater.exe", u"updater.exe"},
        {L"updater_test.exe", u"updater_test.exe"},
        {L"UpdaterSetup.exe", u"UpdaterSetup.exe"},
        {L"UpdaterSetup_test.exe", u"UpdaterSetup_test.exe"},
    }));

TEST_P(OriginalFilenameTest, TestCases) {
  base::FilePath out_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  const base::FilePath module_path(out_dir.Append(GetParam().module_name));
  if (!base::PathExists(module_path)) {
    GTEST_SKIP() << module_path;
  }

  const std::unique_ptr<FileVersionInfoWin> version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(module_path);
  ASSERT_NE(version_info, nullptr);
  EXPECT_EQ(version_info->original_filename(),
            GetParam().expected_original_filename);
}

}  // namespace updater
