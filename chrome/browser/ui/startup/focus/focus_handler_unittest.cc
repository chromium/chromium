// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/focus_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/startup/focus/match_candidate.h"
#include "chrome/browser/ui/startup/focus/selector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace focus {

class FocusHandlerTest : public testing::Test {
 public:
  FocusHandlerTest() = default;
  ~FocusHandlerTest() override = default;

 protected:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(FocusHandlerTest, ProcessFocusRequestNoFlag) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  FocusResult result = ProcessFocusRequest(command_line, *profile_);
  EXPECT_EQ(FocusStatus::kNoMatch,
            result.status);  // No flag = no match (nothing to focus)
}

TEST_F(FocusHandlerTest, ProcessFocusRequestEmptySelector) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "");

  FocusResult result = ProcessFocusRequest(command_line, *profile_);
  EXPECT_EQ(FocusStatus::kParseError, result.status);
}

TEST_F(FocusHandlerTest, ProcessFocusRequestInvalidSelector) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "not-a-valid-url");

  FocusResult result = ProcessFocusRequest(command_line, *profile_);
  EXPECT_EQ(FocusStatus::kParseError, result.status);
}

TEST_F(FocusHandlerTest, FocusResultToExitCode) {
  EXPECT_EQ(0, FocusResultToExitCode(FocusResult(FocusStatus::kFocused)));
  EXPECT_EQ(1, FocusResultToExitCode(FocusResult(FocusStatus::kNoMatch)));
  EXPECT_EQ(2, FocusResultToExitCode(FocusResult(FocusStatus::kParseError)));
}

TEST_F(FocusHandlerTest, FocusResultToString) {
  EXPECT_EQ("focused", FocusResultToString(FocusResult(FocusStatus::kFocused)));
  EXPECT_EQ("no_match",
            FocusResultToString(FocusResult(FocusStatus::kNoMatch)));
  EXPECT_EQ("parse_error",
            FocusResultToString(FocusResult(FocusStatus::kParseError)));

  // Test error enum functionality.
  EXPECT_EQ("parse_error: Empty selector string",
            FocusResultToString(FocusResult(
                FocusStatus::kParseError, FocusResult::Error::kEmptySelector)));
  EXPECT_EQ("parse_error: Invalid selector format",
            FocusResultToString(FocusResult(
                FocusStatus::kParseError, FocusResult::Error::kInvalidFormat)));
}

}  // namespace focus
