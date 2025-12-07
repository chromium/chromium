// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/focus_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/startup/focus/focus_result_file_writer.h"
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
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override { profile_.reset(); }

  base::FilePath GetTempFilePath(const std::string& filename) {
    return temp_dir_.GetPath().AppendASCII(filename);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
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

TEST_F(FocusHandlerTest, CreateFocusJsonString_Focused) {
  FocusResult result(FocusStatus::kFocused, "https://example.com",
                     "https://example.com/path");
  std::string json = CreateFocusJsonString(result);

  EXPECT_TRUE(json.find("\"status\":\"focused\"") != std::string::npos);
  EXPECT_TRUE(json.find("\"exit_code\":0") != std::string::npos);
}

TEST_F(FocusHandlerTest, CreateFocusJsonString_NoMatch) {
  FocusResult result(FocusStatus::kNoMatch);
  std::string json = CreateFocusJsonString(result);

  EXPECT_TRUE(json.find("\"status\":\"no_match\"") != std::string::npos);
  EXPECT_TRUE(json.find("\"exit_code\":1") != std::string::npos);
}

TEST_F(FocusHandlerTest, CreateFocusJsonString_ParseError) {
  FocusResult result(FocusStatus::kParseError,
                     FocusResult::Error::kInvalidFormat);
  std::string json = CreateFocusJsonString(result);

  EXPECT_TRUE(json.find("\"status\":\"parse_error\"") != std::string::npos);
  EXPECT_TRUE(json.find("\"error\":\"Invalid selector format\"") !=
              std::string::npos);
  EXPECT_TRUE(json.find("\"exit_code\":2") != std::string::npos);
}

TEST_F(FocusHandlerTest, CreateFocusJsonString_Opened) {
  FocusResult result(FocusStatus::kOpenedFallback, "https://fallback.com");
  std::string json = CreateFocusJsonString(result);

  EXPECT_TRUE(json.find("\"status\":\"opened\"") != std::string::npos);
  EXPECT_TRUE(json.find("\"exit_code\":0") != std::string::npos);
}

TEST_F(FocusHandlerTest, WriteResultToFile) {
  base::FilePath test_file = GetTempFilePath("focus_result.json");

  FocusResult result(FocusStatus::kFocused, "https://example.com",
                     "https://example.com/path");
  WriteResultToFile(test_file.AsUTF8Unsafe(), result);

  // Wait for async write to complete.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::PathExists(test_file));

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(test_file, &file_contents));

  EXPECT_TRUE(file_contents.find("\"status\":\"focused\"") !=
              std::string::npos);
  EXPECT_TRUE(file_contents.find("\"exit_code\":0") != std::string::npos);
}

TEST_F(FocusHandlerTest, IncognitoDoesNotWriteResultFile) {
  base::FilePath test_file = GetTempFilePath("incognito_result.json");

  // Create an incognito profile
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(profile_.get());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "https://example.com");
  command_line.AppendSwitchASCII(switches::kFocusResultFile,
                                 test_file.AsUTF8Unsafe());

  FocusResult result =
      ProcessFocusRequestWithResultFile(command_line, *incognito_profile);

  // Result file should NOT be created in incognito mode
  EXPECT_FALSE(base::PathExists(test_file));
  EXPECT_EQ(FocusStatus::kNoMatch, result.status);  // No tabs to match
}
}  // namespace focus
