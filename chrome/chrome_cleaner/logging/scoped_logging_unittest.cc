// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/scoped_logging.h"

#include <shlobj.h>

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/test/test_branding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

class ScopedLoggingPathTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    app_data_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_LOCAL_APP_DATA, temp_dir_.GetPath());
    csidl_app_data_override_ = std::make_unique<base::ScopedPathOverride>(
        CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA), temp_dir_.GetPath());
  }

  base::FilePath DefaultExpectedLogFile() const {
    return temp_dir_.GetPath()
        .Append(COMPANY_SHORTNAME_STRING)
        .Append(TEST_PRODUCT_SHORTNAME_STRING)
        .Append(base::CommandLine::ForCurrentProcess()
                    ->GetProgram()
                    .BaseName()
                    .ReplaceExtension(L".log"));
  }

 protected:
  base::ScopedTempDir temp_dir_;

  // Override the default chromium LOCAL_APP_DATA.
  std::unique_ptr<base::ScopedPathOverride> app_data_override_;

  // Override the chrome_cleaner specific LOCAL_APP_DATA used by
  // file_path_sanitization.
  std::unique_ptr<base::ScopedPathOverride> csidl_app_data_override_;
};

TEST_F(ScopedLoggingPathTest, Default) {
  EXPECT_EQ(ScopedLogging::GetLogFilePath(L""), DefaultExpectedLogFile());
  EXPECT_EQ(ScopedLogging::GetLogFilePath(L"-suffix"),
            DefaultExpectedLogFile().InsertBeforeExtension(L"-suffix"));
}

TEST_F(ScopedLoggingPathTest, WithOverride) {
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(kTestLoggingPathSwitch,
                                                         temp_dir2.GetPath());

  base::FilePath expected_log_file =
      temp_dir2.GetPath().Append(base::CommandLine::ForCurrentProcess()
                                     ->GetProgram()
                                     .BaseName()
                                     .ReplaceExtension(L".log"));

  EXPECT_EQ(ScopedLogging::GetLogFilePath(L""), expected_log_file);
  EXPECT_EQ(ScopedLogging::GetLogFilePath(L"-suffix"),
            expected_log_file.InsertBeforeExtension(L"-suffix"));
}

TEST_F(ScopedLoggingPathTest, EmptyOverride) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(kTestLoggingPathSwitch);

  // Should be ignored.
  EXPECT_EQ(ScopedLogging::GetLogFilePath(L""), DefaultExpectedLogFile());
  EXPECT_EQ(ScopedLogging::GetLogFilePath(L"-suffix"),
            DefaultExpectedLogFile().InsertBeforeExtension(L"-suffix"));
}

}  // namespace chrome_cleaner
