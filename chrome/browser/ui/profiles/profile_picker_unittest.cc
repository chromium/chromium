// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_picker.h"

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfilePickerTest : public testing::Test {
 public:
  ProfilePickerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

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
};

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_TwoActive) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  TestingProfile* profile2 =
      testing_profile_manager()->CreateTestingProfile("profile2");
  GetProfileAttributes(profile2)->SetActiveTimeToNow();

  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kMultipleProfiles);

  // Should be within the activity time threshold.
  task_environment()->FastForwardBy(base::Days(27));
  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kMultipleProfiles);
}

TEST_F(ProfilePickerTest,
       ShouldShowAtLaunch_MultipleProfiles_Inactive_SeenPicker) {
  testing_profile_manager()->CreateTestingProfile("profile1");
  testing_profile_manager()->CreateTestingProfile("profile2");
  local_state()->SetBoolean(prefs::kBrowserProfilePickerShown, true);

  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kMultipleProfiles);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_OneGuest) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  testing_profile_manager()->CreateTestingProfile("profile2");
  testing_profile_manager()->CreateGuestProfile();

  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kInactiveProfiles);
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

  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kUserOptedOut);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_Inactive) {
  testing_profile_manager()->CreateTestingProfile("profile1");
  testing_profile_manager()->CreateTestingProfile("profile2");

  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kInactiveProfiles);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_Expired) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  TestingProfile* profile2 =
      testing_profile_manager()->CreateTestingProfile("profile2");
  GetProfileAttributes(profile2)->SetActiveTimeToNow();
  // Should be outside of the activity time threshold.
  task_environment()->FastForwardBy(base::Days(29));

  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kInactiveProfiles);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_MultipleProfiles_OneActive) {
  TestingProfile* profile1 =
      testing_profile_manager()->CreateTestingProfile("profile1");
  GetProfileAttributes(profile1)->SetActiveTimeToNow();
  testing_profile_manager()->CreateTestingProfile("profile2");
  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kInactiveProfiles);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunch_SingleProfile) {
  testing_profile_manager()->CreateTestingProfile("profile1");
  local_state()->SetBoolean(prefs::kBrowserProfilePickerShown, true);
  EXPECT_EQ(ProfilePicker::GetStartupModeReason(),
            StartupProfileModeReason::kSingleProfile);
}

class ProfilePickerParamsTest : public testing::Test {
 public:
  ProfilePickerParamsTest() = default;

  void SetUp() override {
    auto profile_manager_unique = std::make_unique<FakeProfileManager>(
        base::CreateUniqueTempDirectoryScopedToTest());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::move(profile_manager_unique));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ProfilePickerParamsTest, FromEntryPoint_ProfilePath) {
  ProfilePicker::Params params = ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  EXPECT_EQ(base::FilePath(chrome::kSystemProfileDir),
            params.profile_path().BaseName());
}

TEST_F(ProfilePickerParamsTest, CanReuse) {
  ProfilePicker::Params params = ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  EXPECT_TRUE(params.CanReusePickerWindow(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile)));
  EXPECT_TRUE(
      params.CanReusePickerWindow(ProfilePicker::Params::ForBackgroundManager(
          GURL("https://google.com/"))));

  ProfilePicker::Params first_run_params = ProfilePicker::Params::ForFirstRun(
      base::FilePath(FILE_PATH_LITERAL("Profile1")),
      ProfilePicker::FirstRunExitedCallback());
  EXPECT_TRUE(first_run_params.CanReusePickerWindow(first_run_params));
}
