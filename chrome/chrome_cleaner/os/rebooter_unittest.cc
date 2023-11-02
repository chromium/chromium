// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/rebooter.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/test/test_branding.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {
namespace {

const char* kChromeVersionValue = "55.0.0.0";
const char* kTestCleanupId = "test_cleanup_id";
const char* kTestSwitch = "bla";
const char* kTestSwitchName = "bli";
const char* kTestSwitchValue = "blu";
const wchar_t* kExpectedChromeVersionSwitch = L"chrome-version=55.0.0.0";
const wchar_t* kExpectedNoSelfDelete = L"no-self-delete ";
const wchar_t* kExpectedTestSwitch = L"bla ";
const wchar_t* kExpectedTestSwitchValue = L"bli=blu ";

class RebooterTest : public testing::Test {
 public:
  void SetUp() override {
    // Ensure the test starts from a known post-reboot state.
    ASSERT_FALSE(Rebooter::IsPostReboot());
  }
};

TEST_F(RebooterTest, RegisterPostRebootRunAvoidInfiniteLoop) {
  ScopedIsPostReboot is_post_reboot;
  ASSERT_TRUE(Rebooter::IsPostReboot());

  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Rebooter rebooter(TEST_PRODUCT_SHORTNAME_STRING);
  EXPECT_FALSE(rebooter.RegisterPostRebootRun(&command_line, kTestCleanupId,
                                              ExecutionMode::kCleanup,
                                              /*logs_uploads_allowed=*/false));

  // Clean up the state in case the expectation fails.
  rebooter.UnregisterPostRebootRun();
}

TEST_F(RebooterTest, RegisterPostRebootRun) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(kChromeVersionSwitch, kChromeVersionValue);
  command_line.AppendSwitch(kNoSelfDeleteSwitch);

  Rebooter rebooter(TEST_PRODUCT_SHORTNAME_STRING);
  rebooter.AppendPostRebootSwitch(kTestSwitch);
  rebooter.AppendPostRebootSwitchASCII(kTestSwitchName, kTestSwitchValue);
  EXPECT_TRUE(rebooter.RegisterPostRebootRun(&command_line, kTestCleanupId,
                                             ExecutionMode::kCleanup,
                                             /*logs_uploads_allowed=*/false));

  std::string switch_str(kPostRebootSwitchesInOtherRegistryKeySwitch);
  EXPECT_TRUE(RunOnceCommandLineContains(TEST_PRODUCT_SHORTNAME_STRING,
                                         base::UTF8ToWide(switch_str).c_str()));

  EXPECT_TRUE(
      RunOnceOverrideCommandLineContains(kTestCleanupId, kExpectedTestSwitch));
  EXPECT_TRUE(RunOnceOverrideCommandLineContains(kTestCleanupId,
                                                 kExpectedTestSwitchValue));
  EXPECT_TRUE(RunOnceOverrideCommandLineContains(kTestCleanupId,
                                                 kExpectedChromeVersionSwitch));
  EXPECT_TRUE(RunOnceOverrideCommandLineContains(kTestCleanupId,
                                                 kExpectedNoSelfDelete));
  rebooter.UnregisterPostRebootRun();
}

TEST_F(RebooterTest, RegisterPostRebootRun_NotCleanupExecutionMode) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(kChromeVersionSwitch, kChromeVersionValue);
  command_line.AppendSwitch(kNoSelfDeleteSwitch);

  Rebooter rebooter(TEST_PRODUCT_SHORTNAME_STRING);
  rebooter.AppendPostRebootSwitch(kTestSwitch);
  rebooter.AppendPostRebootSwitchASCII(kTestSwitchName, kTestSwitchValue);
  EXPECT_FALSE(rebooter.RegisterPostRebootRun(&command_line, kTestCleanupId,
                                              ExecutionMode::kNone,
                                              /*logs_uploads_allowed=*/false));
}

}  // namespace
}  // namespace chrome_cleaner
