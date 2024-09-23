// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_update.h"

#include <string_view>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

void CreateFileAndWriteData(const base::FilePath& file_path,
                            std::string_view data) {
  ASSERT_TRUE(base::WriteFile(file_path, data));
}

}  // namespace

class WebAppLauncherUpdateTest : public testing::Test {
 protected:
  WebAppLauncherUpdateTest() {}

  void SetUp() override {
    // Create mock chrome_pwa_launcher.exe in <current dir>/<current version>/,
    // where UpdatePwaLaunchers() expects it to be.
    base::FilePath current_dir_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &current_dir_path));
    const base::FilePath mock_version_dir_path =
        current_dir_path.AppendASCII(chrome::kChromeVersion);
    ASSERT_TRUE(mock_version_dir_.Set(mock_version_dir_path));

    latest_version_path_ = mock_version_dir_path.Append(
        FILE_PATH_LITERAL("chrome_pwa_launcher.exe"));
    ASSERT_NO_FATAL_FAILURE(
        CreateFileAndWriteData(latest_version_path_, "up-to-date content"));

    // Create a separate temp directory to store mock launchers, to test
    // launcher update across directories.
    ASSERT_TRUE(mock_launcher_dir_.CreateUniqueTempDir());
    mock_launcher_dir_path_ = mock_launcher_dir_.GetPath();
  }

  void TearDown() override {
    // Fail the test if any created temporary directories can't be deleted.
    EXPECT_TRUE(mock_version_dir_.Delete());
    EXPECT_TRUE(mock_launcher_dir_.Delete());
  }

  // Create a file at path |mock_launcher_dir_path_|/|launcher_name| containing
  // "obsolete content" and add it to |launcher_paths_|.
  void CreateMockLauncher(const base::FilePath::StringType launcher_name) {
    const base::FilePath launcher_path =
        mock_launcher_dir_path_.Append(launcher_name);
    ASSERT_NO_FATAL_FAILURE(
        CreateFileAndWriteData(launcher_path, "obsolete content"));
    launcher_paths_.push_back(launcher_path);
  }

  // Returns true if all launchers' contents match |latest_version_path_|'s
  // contents, false if any launcher's contents do not.
  bool AreLaunchersUpToDate() {
    for (const auto& path : launcher_paths_) {
      if (!base::ContentsEqual(path, latest_version_path_))
        return false;
    }
    return true;
  }

  const std::vector<base::FilePath>& launcher_paths() const {
    return launcher_paths_;
  }

  const base::FilePath& mock_launcher_dir_path() const {
    return mock_launcher_dir_path_;
  }

 private:
  base::FilePath latest_version_path_;
  std::vector<base::FilePath> launcher_paths_;
  base::FilePath mock_launcher_dir_path_;

  base::ScopedTempDir mock_launcher_dir_;
  base::ScopedTempDir mock_version_dir_;
};

TEST_F(WebAppLauncherUpdateTest, UpdatePwaLaunchers) {
  CreateMockLauncher(FILE_PATH_LITERAL("launcher 1"));
  CreateMockLauncher(FILE_PATH_LITERAL("launcher 2"));
  ASSERT_FALSE(AreLaunchersUpToDate());

  UpdatePwaLaunchers(launcher_paths());

  ASSERT_TRUE(AreLaunchersUpToDate());
}

TEST_F(WebAppLauncherUpdateTest, DeleteOldLauncherVersions) {
  CreateMockLauncher(FILE_PATH_LITERAL("launcher"));
  const base::FilePath old_launcher_1 =
      mock_launcher_dir_path().Append(FILE_PATH_LITERAL("launcher_old (1)"));
  const base::FilePath old_launcher_3 =
      mock_launcher_dir_path().Append(FILE_PATH_LITERAL("launcher_old (3)"));
  CreateFileAndWriteData(old_launcher_1, "");
  CreateFileAndWriteData(old_launcher_3, "");
  ASSERT_TRUE(base::PathExists(old_launcher_1));
  ASSERT_TRUE(base::PathExists(old_launcher_3));

  UpdatePwaLaunchers(launcher_paths());

  ASSERT_FALSE(base::PathExists(old_launcher_1));
  ASSERT_FALSE(base::PathExists(old_launcher_3));
}

}  // namespace web_app
