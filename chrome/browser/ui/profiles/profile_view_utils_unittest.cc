// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_view_utils.h"

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileViewUtilsTest : public testing::Test {
 public:
  ProfileViewUtilsTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileViewUtilsTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager()->CreateTestingProfile("profile");
    Test::SetUp();
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

// Assert the utility correctly reports whether the OTR affordance may be shown
// when the incognito policy is applied.
TEST_F(ProfileViewUtilsTest, IsOpenLinkOTREnabled_RespectsIncognitoPolicy) {
  const GURL test_url("https://www.foo.com/");
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  EXPECT_FALSE(IsOpenLinkOTREnabled(profile(), test_url));

  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kEnabled);
  EXPECT_TRUE(IsOpenLinkOTREnabled(profile(), test_url));
}

// Assert the utility correctly reports the OTR affordance may not be shown for
// a OTR source profile.
TEST_F(ProfileViewUtilsTest, IsOpenLinkOTREnabled_DisabledForOTRProfile) {
  const GURL test_url("https://www.foo.com/");
  EXPECT_TRUE(IsOpenLinkOTREnabled(profile(), test_url));

  TestingProfile::Builder incognito_builder;
  TestingProfile* incognito_profile =
      incognito_builder.BuildIncognito(profile());
  EXPECT_FALSE(IsOpenLinkOTREnabled(incognito_profile, test_url));
}
