// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_picker.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfilePickerTest : public testing::Test {
 public:
  ProfilePickerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(testing_profile_manager_.SetUp()); }

  ProfileAttributesEntry& GetProfileAttributes(Profile& profile) {
    return CHECK_DEREF(
        CHECK_DEREF(testing_profile_manager().profile_attributes_storage())
            .GetProfileAttributesWithPath(profile.GetPath()));
  }

  ProfileAttributesEntry& GetProfileAttributes(Profile* profile) {
    return GetProfileAttributes(CHECK_DEREF(profile));
  }

  TestingProfileManager& testing_profile_manager() {
    return testing_profile_manager_;
  }

  PrefService& local_state() {
    return CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state());
  }

  signin::IdentityManager& GetIdentityManager(Profile& profile) {
    return CHECK_DEREF(IdentityManagerFactory::GetForProfile(&profile));
  }

  syncer::TestSyncService& GetSyncService(Profile& profile) {
    return CHECK_DEREF(static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(&profile)));
  }

  TestingProfile& CreateTestingProfileWithSyncService(
      std::string_view name = "test_profile") {
    return CHECK_DEREF(testing_profile_manager().CreateTestingProfile(
        std::string(name),
        TestingProfile::TestingFactories{TestingProfile::TestingFactory{
            SyncServiceFactory::GetInstance(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<syncer::TestSyncService>();
                })}}));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(ProfilePickerTest, ShouldShowAtLaunchMultipleProfilesDefault) {
  testing_profile_manager().CreateTestingProfile("profile1");
  testing_profile_manager().CreateTestingProfile("profile2");
  ASSERT_TRUE(
      local_state().GetBoolean(prefs::kBrowserShowProfilePickerOnStartup));

  EXPECT_EQ(ProfilePicker::GetStartupMode(),
            StartupProfileMode::kProfilePicker);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunchMultipleProfilesDisabledStartup) {
  testing_profile_manager().CreateTestingProfile("profile1");
  testing_profile_manager().CreateTestingProfile("profile2");
  local_state().SetBoolean(prefs::kBrowserShowProfilePickerOnStartup, false);

  EXPECT_EQ(ProfilePicker::GetStartupMode(),
            StartupProfileMode::kBrowserWindow);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunchSingleProfile) {
  testing_profile_manager().CreateTestingProfile("profile1");
  ASSERT_TRUE(
      local_state().GetBoolean(prefs::kBrowserShowProfilePickerOnStartup));

  EXPECT_EQ(ProfilePicker::GetStartupMode(),
            StartupProfileMode::kBrowserWindow);
}

TEST_F(ProfilePickerTest, ShouldShowAtLaunchSingleProfileDisabledStartup) {
  testing_profile_manager().CreateTestingProfile("profile1");
  local_state().SetBoolean(prefs::kBrowserShowProfilePickerOnStartup, false);

  EXPECT_EQ(ProfilePicker::GetStartupMode(),
            StartupProfileMode::kBrowserWindow);
}

TEST_F(ProfilePickerTest,
       ShouldShowAtLaunchProfileEmailSwitchCreateProfileNoMatchingProfile) {
  {
    TestingProfile* profile1 =
        testing_profile_manager().CreateTestingProfile("profile1");
    GetProfileAttributes(profile1).SetAuthInfo(GaiaId("foo"),
                                               u"personal@gmail.com", true);

    EXPECT_EQ(ProfilePicker::GetStartupMode(),
              StartupProfileMode::kBrowserWindow);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kProfileEmail, "test@corp.com");
    EXPECT_EQ(ProfilePicker::GetStartupMode(),
              StartupProfileMode::kBrowserWindow);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCreateProfileEmailIfNotExists);
    EXPECT_EQ(ProfilePicker::GetStartupMode(),
              StartupProfileMode::kProfilePicker);
  }
}

TEST_F(ProfilePickerTest,
       ShouldNotShowAtLaunchProfileEmailSwitchCreateProfileExistingProfile) {
  {
    TestingProfile* profile1 =
        testing_profile_manager().CreateTestingProfile("profile1");
    GetProfileAttributes(profile1).SetAuthInfo(GaiaId("foo"), u"test@corp.com",
                                               true);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kProfileEmail, "test@corp.com");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCreateProfileEmailIfNotExists);
    EXPECT_EQ(ProfilePicker::GetStartupMode(),
              StartupProfileMode::kBrowserWindow);
  }
}

TEST_F(ProfilePickerTest,
       ShouldNotShowAtLaunchProfileEmailSwitchCreateProfileMultipleProfiles) {
  {
    TestingProfile* profile1 =
        testing_profile_manager().CreateTestingProfile("profile1");
    GetProfileAttributes(profile1).SetAuthInfo(GaiaId("foo"), u"test@corp.com",
                                               true);
    TestingProfile* profile2 =
        testing_profile_manager().CreateTestingProfile("profile2");
    GetProfileAttributes(profile2).SetAuthInfo(GaiaId("foo"), u"test2@corp.com",
                                               true);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kProfileEmail, "test@corp.com");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCreateProfileEmailIfNotExists);
    EXPECT_EQ(ProfilePicker::GetStartupMode(),
              StartupProfileMode::kBrowserWindow);
  }
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

TEST_F(ProfilePickerParamsTest, FromEntryPointProfilePath) {
  ProfilePicker::Params params = ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  EXPECT_EQ(base::FilePath(chrome::kSystemProfileDir),
            params.profile_path().BaseName());
}

TEST_F(ProfilePickerParamsTest, FromStartupWithEmail) {
  const std::string kEmail = "test@gmail.com";
  ProfilePicker::Params params =
      ProfilePicker::Params::FromStartupWithEmail(kEmail);
  EXPECT_EQ(base::FilePath(chrome::kSystemProfileDir),
            params.profile_path().BaseName());
  EXPECT_EQ(params.initial_email(), kEmail);
  EXPECT_EQ(params.entry_point(),
            ProfilePicker::EntryPoint::kOnStartupCreateProfileWithEmail);
}

TEST_F(ProfilePickerParamsTest, CanReuse) {
  ProfilePicker::Params params = ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles);
  EXPECT_TRUE(params.CanReusePickerWindow(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile)));
  EXPECT_TRUE(params.CanReusePickerWindow(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOnStartupCreateProfileWithEmail)));
  EXPECT_TRUE(
      params.CanReusePickerWindow(ProfilePicker::Params::ForBackgroundManager(
          GURL("https://google.com/"))));

  ProfilePicker::Params first_run_params = ProfilePicker::Params::ForFirstRun(
      base::FilePath(FILE_PATH_LITERAL("Profile1")),
      ProfilePicker::FirstRunExitedCallback());
  EXPECT_TRUE(first_run_params.CanReusePickerWindow(first_run_params));
  EXPECT_FALSE(params.CanReusePickerWindow(first_run_params));
  EXPECT_FALSE(first_run_params.CanReusePickerWindow(params));

  ProfilePicker::Params glic_manager_params =
      ProfilePicker::Params::ForGlicManager(base::DoNothing());
  EXPECT_TRUE(glic_manager_params.CanReusePickerWindow(glic_manager_params));
  EXPECT_FALSE(params.CanReusePickerWindow(glic_manager_params));
  EXPECT_FALSE(glic_manager_params.CanReusePickerWindow(params));
}

TEST_F(ProfilePickerTest, ComputeFirstRunSkipReasonDefault) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile), std::nullopt);
}

TEST_F(ProfilePickerTest, ComputeFirstRunSkipReasonForceSignin) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();
  signin_util::ScopedForceSigninSetterForTesting scoped_force_signin(true);
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kForceSignin);
}

TEST_F(ProfilePickerTest, ComputeFirstRunSkipReasonProfileAlreadySetUp) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();
  signin::MakePrimaryAccountAvailable(&GetIdentityManager(profile),
                                      "user@gmail.com",
                                      signin::ConsentLevel::kSignin);
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kProfileAlreadySetUp);
}

TEST_F(ProfilePickerTest, ComputeFirstRunSkipReasonPromotionsDisabled) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();
  local_state().SetBoolean(prefs::kPromotionsEnabled, false);
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kSkippedByPolicies);
  local_state().ClearPref(prefs::kPromotionsEnabled);
}

TEST_F(ProfilePickerTest, ComputeFirstRunSkipReasonSyncDisabled) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();
  GetSyncService(profile).SetAllowedByEnterprisePolicy(false);
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kSkippedByPolicies);
}

TEST_F(ProfilePickerTest, ComputeFirstRunSkipReasonSigninDisallowed) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();
  profile.GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kSkippedByPolicies);
}

TEST_F(ProfilePickerTest,
       ComputeFirstRunSkipReasonSigninDisallowedOnNextStartup) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();
  profile.GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup, false);
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kSkippedByPolicies);
}

TEST_F(ProfilePickerTest, ComputeFirstRunSkipReasonPrecedence) {
  TestingProfile& profile = CreateTestingProfileWithSyncService();

  // Both signed in and policy disabled: already set up should take precedence
  // over policy skip.
  signin::MakePrimaryAccountAvailable(&GetIdentityManager(profile),
                                      "user@gmail.com",
                                      signin::ConsentLevel::kSignin);
  GetSyncService(profile).SetAllowedByEnterprisePolicy(false);
  ASSERT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kProfileAlreadySetUp);

  // Force signin should take precedence over already set up.
  signin_util::ScopedForceSigninSetterForTesting scoped_force_signin(true);
  EXPECT_EQ(ProfilePicker::ComputeFirstRunSkipReason(profile),
            ProfilePicker::FirstRunFinishReason::kForceSignin);
}
