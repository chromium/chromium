// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {
const char kUsername[] = "test@test.com";

const char kValidWildcardPattern[] = ".*@test.com";
const char kInvalidWildcardPattern[] = "*@test.com";

const char kMatchingPattern1[] = "test@test.com";
const char kMatchingPattern2[] = ".*@test.com";
const char kMatchingPattern3[] = "test@.*.com";
const char kMatchingPattern4[] = ".*@.*.com";
const char kMatchingPattern5[] = ".*@.*";
const char kMatchingPattern6[] = ".*";

const char kNonMatchingPattern[] = ".*foo.*";
const char kNonMatchingUsernamePattern[] = "foo@test.com";
const char kNonMatchingDomainPattern[] = "test@foo.com";
}  // namespace

class IdentityUtilsIsUsernameAllowedTest : public testing::Test {
 public:
  IdentityUtilsIsUsernameAllowedTest() {
    prefs_.registry()->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                                          std::string());
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

class IdentityUtilsTest : public testing::Test {
 public:
  IdentityUtilsTest()
      : identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &pref_service_) {}

  AccountInfo MakePrimaryAccountAvailable() {
    static const std::string kTestEmail = "test@gmail.com";
    return identity_test_env_.MakePrimaryAccountAvailable(
        kTestEmail, ConsentLevel::kSignin);
  }

  void SetExplicitBrowserSigninPref(bool value) {
    pref_service_.SetBoolean(prefs::kExplicitBrowserSignin, value);
  }

  bool GetExplicitBrowserSigninPref() {
    return pref_service_.GetBoolean(prefs::kExplicitBrowserSignin);
  }

  IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IdentityTestEnvironment identity_test_env_;
};

TEST_F(IdentityUtilsTest, AreGoogleCookiesRebuiltAfterClearingWhenSignedIn) {
  // Signed out.
  EXPECT_TRUE(AreGoogleCookiesRebuiltAfterClearingWhenSignedIn(
      *identity_manager(), *pref_service()));
  // Implicit signin.
  MakePrimaryAccountAvailable();
  SetExplicitBrowserSigninPref(false);
  EXPECT_FALSE(AreGoogleCookiesRebuiltAfterClearingWhenSignedIn(
      *identity_manager(), *pref_service()));
  // Explicit signin.
  SetExplicitBrowserSigninPref(true);
  EXPECT_TRUE(AreGoogleCookiesRebuiltAfterClearingWhenSignedIn(
      *identity_manager(), *pref_service()));
  // Sync.
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      identity_manager()->GetPrimaryAccountId(ConsentLevel::kSignin),
      ConsentLevel::kSync, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  EXPECT_FALSE(AreGoogleCookiesRebuiltAfterClearingWhenSignedIn(
      *identity_manager(), *pref_service()));
}

TEST_F(IdentityUtilsIsUsernameAllowedTest, EmptyPatterns) {
  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, "");
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, "   ");
  EXPECT_FALSE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));
}

TEST_F(IdentityUtilsIsUsernameAllowedTest, InvalidWildcardPatterns) {
  // signin::IsUsernameAllowedByPatternFromPrefs should recognize invalid
  // wildcard patterns like "*@foo.com" and insert a "." before them
  // automatically.
  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kValidWildcardPattern);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kInvalidWildcardPattern);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));
}

TEST_F(IdentityUtilsIsUsernameAllowedTest, MatchingWildcardPatterns) {
  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern1);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern2);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern3);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern4);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern5);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern6);
  EXPECT_TRUE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kNonMatchingPattern);
  EXPECT_FALSE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kNonMatchingUsernamePattern);
  EXPECT_FALSE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kNonMatchingDomainPattern);
  EXPECT_FALSE(IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));
}

TEST_F(IdentityUtilsTest, GetAllGaiaIdsForKeyedPreferences) {
  const int cookie_accounts_count = 3;
  std::vector<gaia::ListedAccount> cookie_accounts(cookie_accounts_count);
  for (int i = 0; i < cookie_accounts_count; ++i) {
    cookie_accounts[i].gaia_id = base::NumberToString(i);
  }
  // Mark one account as signed out and another one as invalid to make sure that
  // all accounts are handled correctly, regardless of their status.
  cookie_accounts[1].signed_out = true;
  cookie_accounts[2].valid = false;

  // No accounts in cookie, no identity manager.
  EXPECT_THAT(
      GetAllGaiaIdsForKeyedPreferences(/*identity_manager=*/nullptr,
                                       AccountsInCookieJarInfo(true, {})),
      testing::UnorderedElementsAre());

  // No accounts in cookie, empty identity manager.
  EXPECT_THAT(GetAllGaiaIdsForKeyedPreferences(
                  identity_manager(), AccountsInCookieJarInfo(true, {})),
              testing::UnorderedElementsAre());

  // Signed in cookie, empty identity manager.
  EXPECT_THAT(GetAllGaiaIdsForKeyedPreferences(
                  identity_manager(),
                  AccountsInCookieJarInfo(true, {cookie_accounts[0]})),
              testing::UnorderedElementsAre("0"));

  // Signed out cookie, empty identity manager.
  EXPECT_THAT(GetAllGaiaIdsForKeyedPreferences(
                  identity_manager(),
                  AccountsInCookieJarInfo(true, {cookie_accounts[1]})),
              testing::UnorderedElementsAre("1"));

  // Signed in, signed out and invalid accounts in cookies, empty identity
  // manager.
  EXPECT_THAT(
      GetAllGaiaIdsForKeyedPreferences(
          identity_manager(),
          AccountsInCookieJarInfo(true, {cookie_accounts[0], cookie_accounts[1],
                                         cookie_accounts[2]})),
      testing::UnorderedElementsAre("0", "1", "2"));

  AccountInfo account_info = MakePrimaryAccountAvailable();
  gaia::ListedAccount cookie_for_primary_account;
  cookie_for_primary_account.gaia_id = account_info.gaia;

  // No accounts in cookie, primary account in identity manager.
  EXPECT_THAT(GetAllGaiaIdsForKeyedPreferences(
                  identity_manager(), AccountsInCookieJarInfo(true, {})),
              testing::UnorderedElementsAre(account_info.gaia));

  // Primary account is valid in cookies.
  EXPECT_THAT(GetAllGaiaIdsForKeyedPreferences(
                  identity_manager(),
                  AccountsInCookieJarInfo(
                      true, {cookie_for_primary_account, cookie_accounts[0],
                             cookie_accounts[1]})),
              testing::UnorderedElementsAre(account_info.gaia, "0", "1"));

  // Primary account is invalid in cookies.
  gaia::ListedAccount cookie_invalid_primary_account;
  cookie_invalid_primary_account.gaia_id = account_info.gaia;
  cookie_invalid_primary_account.valid = false;
  EXPECT_THAT(
      GetAllGaiaIdsForKeyedPreferences(
          identity_manager(),
          AccountsInCookieJarInfo(true, {cookie_accounts[0], cookie_accounts[1],
                                         cookie_invalid_primary_account})),
      testing::UnorderedElementsAre(account_info.gaia, "0", "1"));

  // Primary account is signed out in cookies.
  gaia::ListedAccount cookie_signed_out_primary_account;
  cookie_signed_out_primary_account.gaia_id = account_info.gaia;
  cookie_signed_out_primary_account.signed_out = true;
  EXPECT_THAT(
      GetAllGaiaIdsForKeyedPreferences(
          identity_manager(),
          AccountsInCookieJarInfo(true, {cookie_accounts[0], cookie_accounts[1],
                                         cookie_signed_out_primary_account})),
      testing::UnorderedElementsAre(account_info.gaia, "0", "1"));
}

class IdentityUtilsIsImplicitBrowserSigninOrExplicitDisabled
    : public testing::Test,
      public base::test::WithFeatureOverride {
 public:
  IdentityUtilsIsImplicitBrowserSigninOrExplicitDisabled()
      : base::test::WithFeatureOverride(
            switches::kExplicitBrowserSigninUIOnDesktop),
        identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &pref_service_) {}

  bool IsExplicitBrowserSigninDisabled() const {
    return !IsParamFeatureEnabled();
  }

  void MakePrimaryAccountAvailable() {
    static const std::string kTestEmail = "test@gmail.com";
    identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                   ConsentLevel::kSignin);
  }

  void SetExplicitBrowserSigninPref(bool value) {
    pref_service_.SetBoolean(prefs::kExplicitBrowserSignin, value);
  }

  bool GetExplicitBrowserSigninPref() {
    return pref_service_.GetBoolean(prefs::kExplicitBrowserSignin);
  }

  IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IdentityTestEnvironment identity_test_env_;
};

TEST_P(IdentityUtilsIsImplicitBrowserSigninOrExplicitDisabled,
       NoPrimaryAccount) {
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_FALSE(GetExplicitBrowserSigninPref());
  EXPECT_EQ(IsImplicitBrowserSigninOrExplicitDisabled(identity_manager(),
                                                      pref_service()),
            IsExplicitBrowserSigninDisabled());
}

TEST_P(IdentityUtilsIsImplicitBrowserSigninOrExplicitDisabled,
       PrimaryAccountExplicitSignin) {
  MakePrimaryAccountAvailable();
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  SetExplicitBrowserSigninPref(true);
  ASSERT_TRUE(GetExplicitBrowserSigninPref());

  EXPECT_EQ(IsImplicitBrowserSigninOrExplicitDisabled(identity_manager(),
                                                      pref_service()),
            IsExplicitBrowserSigninDisabled());
}

// Test for users that are already signed in implicitly.
TEST_P(IdentityUtilsIsImplicitBrowserSigninOrExplicitDisabled,
       PrimaryAccountDiceImplicitSignin) {
  MakePrimaryAccountAvailable();
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  SetExplicitBrowserSigninPref(false);
  ASSERT_FALSE(GetExplicitBrowserSigninPref());

  EXPECT_TRUE(IsImplicitBrowserSigninOrExplicitDisabled(identity_manager(),
                                                        pref_service()));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    IdentityUtilsIsImplicitBrowserSigninOrExplicitDisabled);

}  // namespace signin
