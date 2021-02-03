// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_picker.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfilePickerTest : public testing::Test {
 public:
  ProfilePickerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    feature_list_.InitAndEnableFeature(features::kNewProfilePicker);
  }

  void SetUp() override { ASSERT_TRUE(testing_profile_manager_.SetUp()); }

  ProfileAttributesEntry* GetProfileAttributes(Profile* profile) {
    return testing_profile_manager()
        ->profile_attributes_storage()
        ->GetProfileAttributesWithPath(profile->GetPath());
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  PrefService* local_state() {
    return testing_profile_manager()->local_state()->Get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager testing_profile_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_TwoActive) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  TestingProfile* profile2 =
      testing_profile_manager()->CreateTestingProfile("profile2");
  GetProfileAttributes(profile2)->SetActiveTimeToNow();

  EXPECT_TRUE(ProfilePicker::ShouldShowAtLaunch());

  // Should be within the activity time threshold.
  task_environment()->FastForwardBy(base::TimeDelta::FromDays(27));
  EXPECT_TRUE(ProfilePicker::ShouldShowAtLaunch());
}
TEST_F(ProfilePickerTest, ShouldShowAtLaunch_KillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableProfilePickerOnStartupFeature);

  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  TestingProfile* profile2 =
      testing_profile_manager()->CreateTestingProfile("profile2");
  GetProfileAttributes(profile2)->SetActiveTimeToNow();
  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTest,
       ShouldShowAtLaunch_MultipleProfiles_Inactive_SeenPicker) {
  testing_profile_manager()->CreateTestingProfile("profile1");
  testing_profile_manager()->CreateTestingProfile("profile2");
  local_state()->SetBoolean(prefs::kBrowserProfilePickerShown, true);

  EXPECT_TRUE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_OneGuest) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  testing_profile_manager()->CreateTestingProfile("profile2");
  testing_profile_manager()->CreateGuestProfile();

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTest,
       ShouldShowAtLaunch_MultipleProfiles_TwoActive_Disabled) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  TestingProfile* profile2 =
      testing_profile_manager()->CreateTestingProfile("profile2");
  GetProfileAttributes(profile2)->SetActiveTimeToNow();
  local_state()->SetBoolean(prefs::kBrowserShowProfilePickerOnStartup, false);

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_Inactive) {
  testing_profile_manager()->CreateTestingProfile("profile1");
  testing_profile_manager()->CreateTestingProfile("profile2");

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_Expired) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  TestingProfile* profile2 =
      testing_profile_manager()->CreateTestingProfile("profile2");
  GetProfileAttributes(profile2)->SetActiveTimeToNow();
  // Should be outside of the activity time threshold.
  task_environment()->FastForwardBy(base::TimeDelta::FromDays(29));

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_OneActive) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  testing_profile_manager()->CreateTestingProfile("profile2");

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_SingleProfile) {
  testing_profile_manager()->CreateTestingProfile("profile1");
  local_state()->SetBoolean(prefs::kBrowserProfilePickerShown, true);

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

class ProfilePickerTestEphemeralGuest : public ProfilePickerTest {
 public:
  ProfilePickerTestEphemeralGuest() {
    feature_list_.InitAndEnableFeature(
        features::kEnableEphemeralGuestProfilesOnDesktop);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ProfilePickerTestEphemeralGuest,
       ShouldShowAtLaunch_MultipleProfiles_OneGuest) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  testing_profile_manager()->CreateTestingProfile("profile2");
  TestingProfile* guest_profile =
      testing_profile_manager()->CreateGuestProfile();
  GetProfileAttributes(guest_profile)->SetActiveTimeToNow();

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}

TEST_F(ProfilePickerTestEphemeralGuest,
       ShouldShowAtLaunch_MultipleProfiles_OneGuest_SeenPicker) {
  testing_profile_manager()->CreateTestingProfile("profile1");
  testing_profile_manager()->CreateGuestProfile();
  local_state()->SetBoolean(prefs::kBrowserProfilePickerShown, true);

  EXPECT_FALSE(ProfilePicker::ShouldShowAtLaunch());
}
