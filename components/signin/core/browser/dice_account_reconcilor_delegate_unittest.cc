// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {
namespace {
constexpr char kPrimaryAccountEmail[] = "primary@gmail.com ";

gaia::ListedAccount GetListedAccountFromAccountInfo(
    const AccountInfo& account_info,
    bool valid = true) {
  gaia::ListedAccount gaia_account;
  gaia_account.id = account_info.account_id;
  gaia_account.email = account_info.email;
  gaia_account.gaia_id = account_info.gaia;
  gaia_account.valid = valid;
  return gaia_account;
}

class DiceAccountReconcilorDelegateTest
    : public base::test::WithFeatureOverride,
      public testing::Test {
 public:
  DiceAccountReconcilorDelegateTest()
      : base::test::WithFeatureOverride(
            switches::kExplicitBrowserSigninUIOnDesktop),
        identity_test_environment_(nullptr, &pref_service_, nullptr),
        delegate_(identity_manager(),
                  identity_test_environment_.signin_client()) {}

  DiceAccountReconcilorDelegate& delegate() { return delegate_; }

  IdentityTestEnvironment& identity_test_environment() {
    return identity_test_environment_;
  }

  bool IsExplicitBrowserSigninEnabled() const {
    return IsParamFeatureEnabled();
  }

  IdentityManager* identity_manager() {
    return identity_test_environment().identity_manager();
  }

  PrefService* pref_service() { return &pref_service_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IdentityTestEnvironment identity_test_environment_;
  DiceAccountReconcilorDelegate delegate_;
};

TEST_P(DiceAccountReconcilorDelegateTest, GetConsentLevelForPrimaryAccount) {
  ConsentLevel consent_level = IsExplicitBrowserSigninEnabled()
                                   ? ConsentLevel::kSignin
                                   : ConsentLevel::kSync;
  EXPECT_EQ(delegate().GetConsentLevelForPrimaryAccount(), consent_level);

  if (IsExplicitBrowserSigninEnabled()) {
    // Sign in.
    identity_test_environment().MakePrimaryAccountAvailable(
        "test@gmail.com", ConsentLevel::kSignin);
    // Simulate Dice User migrating.
    pref_service()->SetBoolean(prefs::kExplicitBrowserSignin, false);

    // The behavior for Dice users migrating should be the same as if the
    // feature is disabled.
    EXPECT_EQ(delegate().GetConsentLevelForPrimaryAccount(),
              ConsentLevel::kSync);
  }
}

TEST_P(DiceAccountReconcilorDelegateTest,
       IsCookieBasedConsistencyModePreChromeSignIn) {
  EXPECT_EQ(delegate().IsCookieBasedConsistencyMode(),
            IsExplicitBrowserSigninEnabled());
}

TEST_P(DiceAccountReconcilorDelegateTest,
       IsCookieBasedConsistencyModePostChromeSignIn) {
  identity_test_environment().MakePrimaryAccountAvailable(
      kPrimaryAccountEmail, ConsentLevel::kSignin);
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_FALSE(delegate().IsCookieBasedConsistencyMode());
}

TEST_P(DiceAccountReconcilorDelegateTest,
       RevokeSecondaryTokensForReconcileIfNeededPreChromeSignIn) {
  if (!IsExplicitBrowserSigninEnabled()) {
    GTEST_SKIP();
  }
  AccountInfo valid_account =
      identity_test_environment().MakeAccountAvailable("account@gmail.com");

  // Accounts with invalid refresh token should be revoked.
  AccountInfo account_with_invalid_refresh_token =
      identity_test_environment().MakeAccountAvailable(
          AccountAvailabilityOptionsBuilder()
              .WithRefreshToken(GaiaConstants::kInvalidRefreshToken)
              .Build("invalid_refresh_token@gmail.com"));

  // Accounts not in the gaia cookie should be revoked.
  AccountInfo no_cookie_account =
      identity_test_environment().MakeAccountAvailable("no_cookie@gmail.com");
  AccountInfo cookie_no_refresh_token_account =
      identity_test_environment().MakeAccountAvailable(
          AccountAvailabilityOptionsBuilder().WithoutRefreshToken().Build(
              "no_refresh_token_with_cookie@gmail.com"));

  // Setup account with refresh token and invalid cookie account.
  // The refresh token should be revoked
  AccountInfo invalid_cookie_account =
      identity_test_environment().MakeAccountAvailable(
          "invalid_cookie@gmail.com");

  // Verify the test setup.
  EXPECT_THAT(identity_manager()->GetAccountsWithRefreshTokens(),
              ::testing::UnorderedElementsAre(
                  valid_account, account_with_invalid_refresh_token,
                  no_cookie_account, invalid_cookie_account));
  ASSERT_TRUE(delegate().IsCookieBasedConsistencyMode());

  const std::vector<gaia::ListedAccount> gaia_signed_in_accounts{
      GetListedAccountFromAccountInfo(valid_account),
      GetListedAccountFromAccountInfo(cookie_no_refresh_token_account),
      GetListedAccountFromAccountInfo(invalid_cookie_account, /*valid=*/false)};

  delegate().RevokeSecondaryTokensForReconcileIfNeeded(gaia_signed_in_accounts);
  std::vector<CoreAccountInfo> chrome_accounts =
      identity_manager()->GetAccountsWithRefreshTokens();
  ASSERT_EQ(chrome_accounts.size(), 1u);
  EXPECT_THAT(chrome_accounts[0], ::testing::Eq(valid_account));
}

TEST_P(DiceAccountReconcilorDelegateTest, RevokeSecondaryTokensForReconcile) {
  AccountInfo valid_account =
      identity_test_environment().MakeAccountAvailable("account@gmail.com");

  if (IsExplicitBrowserSigninEnabled()) {
    identity_test_environment().SetPrimaryAccount(valid_account.email,
                                                  ConsentLevel::kSignin);
  }
  EXPECT_FALSE(delegate().IsCookieBasedConsistencyMode());
  // Accounts with invalid refresh token should be revoked.
  AccountInfo account_with_invalid_refresh_token =
      identity_test_environment().MakeAccountAvailable(
          AccountAvailabilityOptionsBuilder()
              .WithRefreshToken(GaiaConstants::kInvalidRefreshToken)
              .Build("invalid_refresh_token@gmail.com"));

  // Only accounts with invalid refresh tokens should be revoked.
  AccountInfo no_cookie_account =
      identity_test_environment().MakeAccountAvailable("no_cookie@gmail.com");
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3u);

  const std::vector<gaia::ListedAccount> gaia_signed_in_accounts{
      GetListedAccountFromAccountInfo(valid_account),
      GetListedAccountFromAccountInfo(account_with_invalid_refresh_token)};
  delegate().RevokeSecondaryTokensForReconcileIfNeeded(gaia_signed_in_accounts);
  EXPECT_THAT(
      identity_manager()->GetAccountsWithRefreshTokens(),
      ::testing::UnorderedElementsAre(valid_account, no_cookie_account));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(DiceAccountReconcilorDelegateTest);

}  // namespace
}  // namespace signin
