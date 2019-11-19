// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/settings.h"

#include <tuple>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

// Test params:
//  - execution_mode (legacy, scanning, or cleanup mode)
//  - with_scanning_mode_logs: whether switch --with-scanning-mode-logs is
//        present on the command line.
//  - with_cleanup_mode_logs: whether switch --with-cleanup-mode-logs is
//        present on the command line.
//  - with_test_logging_url: whether switch --test_logging_url is present on the
//        command line.
//  - uploading_blocked: whether switch --no-report-upload is present on the
//        command line.
class CleanerSettingsTest
    : public testing::TestWithParam<
          std::tuple<ExecutionMode, bool, bool, bool, bool>> {
 protected:
  Settings* ReinitializeSettings(const base::CommandLine& command_line) {
    Settings* settings = Settings::GetInstance();
    settings->Initialize(command_line, TargetBinary::kCleaner);
    return settings;
  }

  // Test params.
  ExecutionMode execution_mode_;
  bool with_scanning_mode_logs_;
  bool with_cleanup_mode_logs_;
  bool with_test_logging_url_;
  bool uploading_blocked_;
};

TEST_P(CleanerSettingsTest, CleanerLogsPermissions) {
  std::tie(execution_mode_, with_scanning_mode_logs_, with_cleanup_mode_logs_,
           with_test_logging_url_, uploading_blocked_) = GetParam();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  if (execution_mode_ != ExecutionMode::kNone) {
    command_line.AppendSwitchASCII(
        kExecutionModeSwitch,
        base::NumberToString(static_cast<int>(execution_mode_)));
  }
  if (with_scanning_mode_logs_)
    command_line.AppendSwitch(kWithScanningModeLogsSwitch);
  if (with_cleanup_mode_logs_)
    command_line.AppendSwitch(kWithCleanupModeLogsSwitch);
  if (with_test_logging_url_)
    command_line.AppendSwitch(kTestLoggingURLSwitch);
  if (uploading_blocked_)
    command_line.AppendSwitch(kNoReportUploadSwitch);

  Settings* settings = ReinitializeSettings(command_line);

  const bool expect_logs_collection_enabled =
      (execution_mode_ == ExecutionMode::kScanning &&
       with_scanning_mode_logs_) ||
      (execution_mode_ == ExecutionMode::kCleanup && with_cleanup_mode_logs_);
  EXPECT_EQ(expect_logs_collection_enabled,
            settings->logs_collection_enabled());

  bool logs_upload_allowed =
      expect_logs_collection_enabled && !uploading_blocked_;
#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  if (!with_test_logging_url_)
    logs_upload_allowed = false;
#endif
  EXPECT_EQ(logs_upload_allowed, settings->logs_upload_allowed());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CleanerSettingsTest,
    ::testing::Combine(
        /*execution_mode=*/::testing::Values(ExecutionMode::kNone,
                                             ExecutionMode::kScanning,
                                             ExecutionMode::kCleanup),
        /*with_scanning_mode_logs=*/::testing::Bool(),
        /*with_cleanup_mode_logs=*/::testing::Bool(),
        /*with_test_logging_url=*/::testing::Bool(),
        /*uploading_blocked=*/::testing::Bool()),
    GetParamNameForTest());

}  // namespace chrome_cleaner
