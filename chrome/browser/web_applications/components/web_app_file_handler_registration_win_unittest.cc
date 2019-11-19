// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration_win.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class UpdateChromeExePathTest : public testing::Test {
 protected:
  UpdateChromeExePathTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}

  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
    ASSERT_FALSE(user_data_dir_.empty());
    last_browser_file_ = user_data_dir_.Append(kLastBrowserFile);
  }

  static base::FilePath GetCurrentExePath() {
    base::FilePath current_exe_path;
    EXPECT_TRUE(base::PathService::Get(base::FILE_EXE, &current_exe_path));
    return current_exe_path;
  }

  base::FilePath GetLastBrowserPathFromFile() const {
    std::string last_browser_file_data;
    EXPECT_TRUE(
        base::ReadFileToString(last_browser_file_, &last_browser_file_data));
    base::FilePath::StringPieceType last_browser_path(
        reinterpret_cast<const base::FilePath::CharType*>(
            last_browser_file_data.data()),
        last_browser_file_data.size() / sizeof(base::FilePath::CharType));
    return base::FilePath(last_browser_path);
  }

  const base::FilePath& user_data_dir() const { return user_data_dir_; }

 private:
  // Redirect |chrome::DIR_USER_DATA| to a temporary directory during testing.
  base::ScopedPathOverride user_data_dir_override_;

  base::FilePath user_data_dir_;
  base::FilePath last_browser_file_;
};

TEST_F(UpdateChromeExePathTest, UpdateChromeExePath) {
  UpdateChromeExePath(user_data_dir());
  EXPECT_EQ(GetLastBrowserPathFromFile(), GetCurrentExePath());
}

}  // namespace web_app
