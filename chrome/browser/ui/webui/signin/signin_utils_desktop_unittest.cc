// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"

#include "base/command_line.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

class CanOfferSigninTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("first");
  }

  Profile* profile() { return profile_; }
  Profile* CreateTestingProfile(const std::string& profile_name) {
    return profile_manager_.CreateTestingProfile(profile_name);
  }

  void AllowSigninCookies(bool enable) {
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile()).get();
    cookie_settings->SetDefaultCookieSetting(enable ? CONTENT_SETTING_ALLOW
                                                    : CONTENT_SETTING_BLOCK);
  }

  void SetAllowedUsernamePattern(const std::string& pattern) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetString(
        prefs::kGoogleServicesUsernamePattern, pattern);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
};

TEST_F(CanOfferSigninTest, NoProfile) {
  SigninUIError error =
      CanOfferSignin(nullptr, GaiaId("12345"), "user@gmail.com",
                     /*allow_account_from_other_profile=*/false);
  EXPECT_FALSE(error.IsOk());
  EXPECT_EQ(error, SigninUIError::Other("user@gmail.com"));
}

TEST_F(CanOfferSigninTest, Default) {
  EXPECT_TRUE(CanOfferSignin(profile(), GaiaId("12345"), "user@gmail.com",
                             /*allow_account_from_other_profile=*/false)
                  .IsOk());
}

TEST_F(CanOfferSigninTest, ProfileConnected) {
  const AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()), "foo@gmail.com",
      signin::ConsentLevel::kSync);

  EXPECT_TRUE(CanOfferSignin(profile(), account_info.gaia, account_info.email,
                             /*allow_account_from_other_profile=*/false)
                  .IsOk());
  EXPECT_TRUE(CanOfferSignin(profile(), account_info.gaia, "foo",
                             /*allow_account_from_other_profile=*/false)
                  .IsOk());
  SigninUIError error =
      CanOfferSignin(profile(), account_info.gaia, "user@gmail.com",
                     /*allow_account_from_other_profile=*/false);
  EXPECT_FALSE(error.IsOk());
  EXPECT_EQ(error, SigninUIError::WrongReauthAccount("user@gmail.com",
                                                     account_info.email));
}

TEST_F(CanOfferSigninTest, UsernameNotAllowed) {
  SetAllowedUsernamePattern("*.google.com");

  SigninUIError error =
      CanOfferSignin(profile(), GaiaId("12345"), "foo@gmail.com",
                     /*allow_account_from_other_profile=*/false);
  EXPECT_FALSE(error.IsOk());
  EXPECT_EQ(error, SigninUIError::UsernameNotAllowedByPatternFromPrefs(
                       "foo@gmail.com"));
}

TEST_F(CanOfferSigninTest, NoSigninCookies) {
  AllowSigninCookies(false);

  SigninUIError error =
      CanOfferSignin(profile(), GaiaId("12345"), "user@gmail.com",
                     /*allow_account_from_other_profile=*/false);
  EXPECT_FALSE(error.IsOk());
  EXPECT_EQ(error, SigninUIError::Other("user@gmail.com"));
}

class CanOfferSigninWithSigninToSyncFeatureTest
    : public base::test::WithFeatureOverride,
      public CanOfferSigninTest {
 public:
  CanOfferSigninWithSigninToSyncFeatureTest()
      : base::test::WithFeatureOverride(
            syncer::kReplaceSyncPromosWithSignInPromos) {}
};

TEST_P(CanOfferSigninWithSigninToSyncFeatureTest,
       OtherProfileAlreadySyncingWithAccount) {
  const AccountInfo first_account_info = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()), "foo@gmail.com",
      signin::ConsentLevel::kSync);

  Profile* second_profile = CreateTestingProfile("second");
  SigninUIError error = CanOfferSignin(
      second_profile, first_account_info.gaia, first_account_info.email,
      /*allow_account_from_other_profile=*/false);
  EXPECT_FALSE(error.IsOk());
  EXPECT_EQ(error, SigninUIError::AccountAlreadyUsedByAnotherProfile(
                       first_account_info.email, profile()->GetPath()));
}

TEST_P(CanOfferSigninWithSigninToSyncFeatureTest,
       OtherProfileAlreadySyncingWithAccountWithBypass) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kBypassAccountAlreadyUsedByAnotherProfileCheck);

  const AccountInfo first_account_info = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()), "foo@gmail.com",
      signin::ConsentLevel::kSync);

  Profile* second_profile = CreateTestingProfile("second");
  SigninUIError error = CanOfferSignin(
      second_profile, first_account_info.gaia, first_account_info.email,
      /*allow_account_from_other_profile=*/false);
  EXPECT_TRUE(error.IsOk());
}

TEST_P(CanOfferSigninWithSigninToSyncFeatureTest,
       OtherProfileAlreadySignedInWithAccount) {
  const AccountInfo first_account_info = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()), "foo@gmail.com",
      signin::ConsentLevel::kSignin);

  Profile* second_profile = CreateTestingProfile("second");
  SigninUIError error = CanOfferSignin(
      second_profile, first_account_info.gaia, first_account_info.email,
      /*allow_account_from_other_profile=*/false);
  if (IsParamFeatureEnabled()) {
    EXPECT_FALSE(error.IsOk());
    EXPECT_EQ(error, SigninUIError::AccountAlreadyUsedByAnotherProfile(
                         first_account_info.email, profile()->GetPath()));
  } else {
    EXPECT_TRUE(error.IsOk());
  }

  EXPECT_TRUE(CanOfferSignin(second_profile, first_account_info.gaia,
                             first_account_info.email,
                             /*allow_account_from_other_profile=*/true)
                  .IsOk());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    CanOfferSigninWithSigninToSyncFeatureTest);
