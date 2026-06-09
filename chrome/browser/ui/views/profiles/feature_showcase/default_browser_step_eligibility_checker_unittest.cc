// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/default_browser_step_eligibility_checker.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestDefaultBrowserStepEligibilityChecker
    : public DefaultBrowserStepEligibilityChecker {
 public:
  TestDefaultBrowserStepEligibilityChecker() = default;

  void SetStateForTesting(shell_integration::DefaultWebClientState state) {
    state_ = state;
  }

 protected:
  void StartCheckIsDefault(
      shell_integration::DefaultWebClientWorkerCallback callback) override {
    std::move(callback).Run(state_);
  }

 private:
  shell_integration::DefaultWebClientState state_ =
      shell_integration::UNKNOWN_DEFAULT;
};

}  // namespace

class DefaultBrowserStepEligibilityCheckerTest : public testing::Test {
 public:
  DefaultBrowserStepEligibilityCheckerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("TestProfile");
  }

  void SetDefaultBrowserDisabledByPolicy(bool disabled) {
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetManagedPref(
        prefs::kDefaultBrowserSettingEnabled, base::Value(!disabled));
  }

 protected:
  Profile& profile() { return CHECK_DEREF(profile_.get()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(DefaultBrowserStepEligibilityCheckerTest, DisabledByPolicy) {
  SetDefaultBrowserDisabledByPolicy(true);

  TestDefaultBrowserStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

#if BUILDFLAG(IS_WIN)
TEST_F(DefaultBrowserStepEligibilityCheckerTest, CheckFinishedIsDefault) {
  SetDefaultBrowserDisabledByPolicy(false);

  TestDefaultBrowserStepEligibilityChecker checker;
  checker.SetStateForTesting(shell_integration::IS_DEFAULT);
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(DefaultBrowserStepEligibilityCheckerTest, CheckFinishedNotDefault) {
  SetDefaultBrowserDisabledByPolicy(false);

  TestDefaultBrowserStepEligibilityChecker checker;
  checker.SetStateForTesting(shell_integration::NOT_DEFAULT);
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(DefaultBrowserStepEligibilityCheckerTest,
       CheckFinishedOtherModeIsDefault) {
  SetDefaultBrowserDisabledByPolicy(false);

  TestDefaultBrowserStepEligibilityChecker checker;
  checker.SetStateForTesting(shell_integration::OTHER_MODE_IS_DEFAULT);
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(DefaultBrowserStepEligibilityCheckerTest, CheckFinishedUnknownDefault) {
  SetDefaultBrowserDisabledByPolicy(false);

  TestDefaultBrowserStepEligibilityChecker checker;
  checker.SetStateForTesting(shell_integration::UNKNOWN_DEFAULT);
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}
#endif

#if !BUILDFLAG(IS_WIN)
TEST_F(DefaultBrowserStepEligibilityCheckerTest, NonWindowsReturnsFalse) {
  SetDefaultBrowserDisabledByPolicy(false);

  TestDefaultBrowserStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}
#endif
