// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

class BrowserUnitTest : public testing::Test {
 public:
  BrowserUnitTest() = default;
  ~BrowserUnitTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BrowserUnitTest, CreateBrowserFailsIfProfileDisallowsBrowserWindows) {
  TestingProfile::Builder profile_builder;
  profile_builder.DisallowBrowserWindows();
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.DisallowBrowserWindows();
  otr_profile_builder.BuildIncognito(test_profile.get());

  // Verify creating browser fails in both original and OTR version of the
  // profile.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(*test_profile.get()));
  EXPECT_EQ(
      BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
      GetBrowserWindowCreationStatusForProfile(
          *test_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

// Tests GetBrowserWindowCreationStatusForProfile() when Incognito mode is
// disabled.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeDisabled) {
  IncognitoModePrefs::SetAvailability(
      profile_.GetPrefs(), policy::IncognitoModeAvailability::kDisabled);

  // Creating a browser window in OTR profile should fail if incognito is
  // disabled.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(
                *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  // Verify creating a browser in the original profile is allowed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(profile_));
}

// Tests GetBrowserWindowCreationStatusForProfile() when Incognito mode is
// forced.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeForced) {
  IncognitoModePrefs::SetAvailability(
      profile_.GetPrefs(), policy::IncognitoModeAvailability::kForced);

  // Creating a browser window in the original profile should fail if incognito
  // is forced.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(profile_));

  // Creating a browser in OTR test profile should succeed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(
                *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

// Tests GetBrowserWindowCreationStatusForProfile() with no restrictions on
// incognito mode.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeEnabled) {
  ASSERT_EQ(policy::IncognitoModeAvailability::kEnabled,
            IncognitoModePrefs::GetAvailability(&profile_));

  // Creating a browser in the original test profile should succeed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(profile_));

  // Creating a browser in OTR test profile should succeed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(
                *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(BrowserUnitTest, CreateBrowserFailsIfProfileIsLocked) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* test_profile =
      profile_manager.CreateTestingProfile("locked_profile");
  ASSERT_TRUE(test_profile);
  Profile* otr_profile =
      test_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  ProfileAttributesEntry* entry =
      profile_manager.profile_attributes_storage()
          ->GetProfileAttributesWithPath(test_profile->GetPath());
  ASSERT_NE(nullptr, entry);

  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(*test_profile));
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(*otr_profile));

  entry->LockForceSigninProfile(true);
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(*test_profile));
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(*otr_profile));

  // Even when Incognito mode is forced by policy, a locked profile cannot open
  // an OTR browser window.
  IncognitoModePrefs::SetAvailability(
      test_profile->GetPrefs(), policy::IncognitoModeAvailability::kForced);
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(*otr_profile));

  // Shutdown status takes precedence over profile suitability errors.
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorShuttingDown,
            GetBrowserWindowCreationStatusForProfile(*test_profile));
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(false);

  // Guest off-the-record sessions have no ProfileAttributesEntry and remain
  // allowed when ForceSignin is active.
  TestingProfile* guest_profile = profile_manager.CreateGuestProfile();
  ASSERT_TRUE(guest_profile);
  Profile* guest_otr =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(guest_otr);
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(*guest_otr));

  IncognitoModePrefs::SetAvailability(
      test_profile->GetPrefs(), policy::IncognitoModeAvailability::kEnabled);
  entry->LockForceSigninProfile(false);
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(*test_profile));
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(*otr_profile));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BrowserUnitTest, CreateBrowserDuringKioskSplashScreen) {
  // Setting up user manager state to be in kiosk mode:
  // Creating a new user manager.
  auto* user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager manager{
      std::unique_ptr<user_manager::UserManager>(user_manager)};

  // Create SessionManager AFTER UserManager so it is destroyed BEFORE it.
  session_manager::SessionManager session_manager{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};

  const user_manager::User* user = user_manager->AddKioskChromeAppUser(
      AccountId::FromUserEmail("fake_user@test"));
  user_manager->LoginUser(user->GetAccountId());

  TestingProfile kiosk_profile;

  session_manager.SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  // Browser should not be created during login session state.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorLoadingKiosk,
            GetBrowserWindowCreationStatusForProfile(kiosk_profile));

  session_manager.SetSessionState(session_manager::SessionState::ACTIVE);
  // Normal flow, creation succeeds.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(kiosk_profile));
}
#endif
