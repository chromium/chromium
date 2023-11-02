// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_features_util.h"

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace features_util {
namespace {

base::Value::Dict CreateOptedInAccountPref() {
  base::Value::Dict global_pref;
  base::Value::Dict account_pref;
  account_pref.Set("opted_in", true);
  global_pref.Set("some_gaia_hash", std::move(account_pref));
  return global_pref;
}
}  // namespace

class PasswordManagerFeaturesUtilTestBase {
 public:
  PasswordManagerFeaturesUtilTestBase() {
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kAccountStoragePerAccountSettings);
  }

 protected:
  // Sets up |sync_service_| for the case where there is no signed-in user (so
  // |sync_service_| will be fully inactive).
  void SetSyncStateNotSignedIn() {
    sync_service_.SetAccountInfo(CoreAccountInfo());
    sync_service_.SetHasSyncConsent(false);
    sync_service_.SetTransportState(
        syncer::SyncService::TransportState::DISABLED);
    sync_service_.SetDisableReasons(
        {syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN});
  }

  // Sets up |sync_service_| for the case where there is a signed-in user, but
  // they have *not* enabled Sync-the-feature. Sync will be active in
  // "transport-only" mode, meaning that the user will be eligibe for
  // account-based features such as the account-scoped password storage.
  void SetSyncStateTransportActive(const CoreAccountInfo& account) {
    sync_service_.SetAccountInfo(account);
    sync_service_.SetHasSyncConsent(false);
    sync_service_.SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service_.SetDisableReasons(
        {syncer::SyncService::DisableReason::DISABLE_REASON_USER_CHOICE});
    ASSERT_FALSE(sync_service_.IsSyncFeatureEnabled());
  }

  // Sets up |sync_service_| for the case where the signed-in user has enabled
  // Sync-the-feature.
  void SetSyncStateFeatureActive(const CoreAccountInfo& account) {
    sync_service_.SetAccountInfo(account);
    sync_service_.SetHasSyncConsent(true);
    sync_service_.SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service_.SetDisableReasons({});
    sync_service_.SetFirstSetupComplete(true);
    ASSERT_TRUE(sync_service_.IsSyncFeatureEnabled());
  }

  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
};

// Test fixture where the account-scoped password storage is *disabled*.
class PasswordManagerFeaturesUtilWithoutAccountStorageTest
    : public PasswordManagerFeaturesUtilTestBase,
      public testing::Test {
 public:
  PasswordManagerFeaturesUtilWithoutAccountStorageTest() {
    features_.InitAndDisableFeature(features::kEnablePasswordsAccountStorage);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Test fixture where the account-scoped password storage is *enabled*.
class PasswordManagerFeaturesUtilTest
    : public PasswordManagerFeaturesUtilTestBase,
      public testing::Test {
 private:
  base::test::ScopedFeatureList features_{
      features::kEnablePasswordsAccountStorage};
};

TEST_F(PasswordManagerFeaturesUtilWithoutAccountStorageTest,
       AccountStoragePerAccountSettings) {
  CoreAccountInfo account;
  account.email = "first@account.com";
  account.gaia = "first";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // SyncService is running in transport mode with |account|.
  SetSyncStateTransportActive(account);

  // Since the account storage feature is disabled, the profile store should be
  // the default.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Same if the user is signed out.
  SetSyncStateNotSignedIn();
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilTest, ShowAccountStorageResignIn) {
  // Add an account to prefs which opted into using the account-storage.
  pref_service_.SetDict(prefs::kAccountStoragePerAccountSettings,
                        CreateOptedInAccountPref());

  // SyncService is not running (because no user is signed-in).
  SetSyncStateNotSignedIn();

  EXPECT_TRUE(
      ShouldShowAccountStorageReSignin(&pref_service_, &sync_service_, GURL()));
}

TEST_F(PasswordManagerFeaturesUtilWithoutAccountStorageTest,
       ShowAccountStorageReSignin) {
  // Add an account to prefs which opted into using the account-storage.
  pref_service_.SetDict(prefs::kAccountStoragePerAccountSettings,
                        CreateOptedInAccountPref());

  // SyncService is not running (because no user is signed-in).
  SetSyncStateNotSignedIn();

  EXPECT_FALSE(
      ShouldShowAccountStorageReSignin(&pref_service_, &sync_service_, GURL()));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       DontShowAccountStorageResignIn_SyncActive) {
  // Add an account to prefs which opted into using the account-storage.
  pref_service_.SetDict(prefs::kAccountStoragePerAccountSettings,
                        CreateOptedInAccountPref());

  // SyncService is running (for a different signed-in user).
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  SetSyncStateTransportActive(account);

  EXPECT_FALSE(
      ShouldShowAccountStorageReSignin(&pref_service_, &sync_service_, GURL()));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       DontShowAccountStorageResignIn_NoPrefs) {
  // Pref is not set for any account.

  // SyncService is not running (because no user is signed-in).
  SetSyncStateNotSignedIn();

  EXPECT_FALSE(
      ShouldShowAccountStorageReSignin(&pref_service_, &sync_service_, GURL()));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       DontShowAccountStorageResignIn_GaiaUrl) {
  // Add an account to prefs which opted into using the account-storage.
  pref_service_.SetDict(prefs::kAccountStoragePerAccountSettings,
                        CreateOptedInAccountPref());

  // SyncService is not running (because no user is signed-in).
  SetSyncStateNotSignedIn();

  // The re-signin promo should show up in contexts without a URL (e.g. native
  // UI).
  EXPECT_TRUE(
      ShouldShowAccountStorageReSignin(&pref_service_, &sync_service_, GURL()));
  // The re-signin promo should show up on all regular pages.
  EXPECT_TRUE(ShouldShowAccountStorageReSignin(&pref_service_, &sync_service_,
                                               GURL("http://www.example.com")));
  EXPECT_TRUE(ShouldShowAccountStorageReSignin(
      &pref_service_, &sync_service_, GURL("https://www.example.com")));
  // The re-signin promo should NOT show up on Google sign-in pages.
  EXPECT_FALSE(ShouldShowAccountStorageReSignin(
      &pref_service_, &sync_service_, GURL("https://accounts.google.com")));
  EXPECT_FALSE(ShouldShowAccountStorageReSignin(
      &pref_service_, &sync_service_,
      GURL("https://accounts.google.com/some/path")));
}

TEST_F(PasswordManagerFeaturesUtilTest, AccountStoragePerAccountSettings) {
  CoreAccountInfo first_account;
  first_account.email = "first@account.com";
  first_account.gaia = "first";
  first_account.account_id = CoreAccountId::FromGaiaId(first_account.gaia);

  CoreAccountInfo second_account;
  second_account.email = "second@account.com";
  second_account.gaia = "second";
  second_account.account_id = CoreAccountId::FromGaiaId(second_account.gaia);

  SetSyncStateNotSignedIn();

  // Initially the user is not signed in, so everything is off/local.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Now let SyncService run in transport mode with |first_account|.
  SetSyncStateTransportActive(first_account);

  // By default, the user is not opted in, but eligible.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_TRUE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Opt in!
  OptInToAccountStorage(&pref_service_, &sync_service_);
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  // Now the default is saving to the account.
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kAccountStore);

  // Change the default store to the profile one.
  SetDefaultPasswordStore(&pref_service_, &sync_service_,
                          PasswordForm::Store::kProfileStore);
  EXPECT_TRUE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Change to |second_account|. The opt-in for |first_account|, and its store
  // choice, should not apply.
  SetSyncStateTransportActive(second_account);
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_TRUE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));

  // Change back to |first_account|. The previous opt-in and chosen default
  // store should now apply again.
  SetSyncStateTransportActive(first_account);
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_TRUE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Sign out. Now the settings should have reasonable default values (not opted
  // in, save to profile store).
  SetSyncStateNotSignedIn();
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilTest,
       AccountStorageKeepSettingsOnlyForUsers) {
  CoreAccountInfo first_account;
  first_account.email = "first@account.com";
  first_account.gaia = "first";
  first_account.account_id = CoreAccountId::FromGaiaId(first_account.gaia);

  CoreAccountInfo second_account;
  second_account.email = "second@account.com";
  second_account.gaia = "second";
  second_account.account_id = CoreAccountId::FromGaiaId(second_account.gaia);

  // Let SyncService run in transport mode with |first_account| and opt in.
  SetSyncStateTransportActive(first_account);
  OptInToAccountStorage(&pref_service_, &sync_service_);
  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // Switch to |second_account| and again opt in.
  SetSyncStateTransportActive(second_account);
  OptInToAccountStorage(&pref_service_, &sync_service_);
  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // Sign out. The opt-in still exists, but doesn't apply anymore.
  SetSyncStateNotSignedIn();
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // Keep the opt-in only for |first_account| (and some unknown other user).
  KeepAccountStorageSettingsOnlyForUsers(&pref_service_,
                                         {first_account.gaia, "other_gaia_id"});

  // The first account should still be opted in, but not the second.
  SetSyncStateTransportActive(first_account);
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  SetSyncStateTransportActive(second_account);
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest, SyncSuppressesAccountStorageOptIn) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // Initially, the user is signed in but doesn't have Sync-the-feature enabled,
  // so the SyncService is running in transport mode.
  SetSyncStateTransportActive(account);

  // In this state, the user could opt in to the account storage.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));

  // Now the user enables Sync-the-feature.
  SetSyncStateFeatureActive(account);
  ASSERT_TRUE(sync_service_.IsSyncFeatureEnabled());

  // Now the account-storage opt-in should *not* be available anymore.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest, SyncDisablesAccountStorage) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in transport mode.
  SetSyncStateTransportActive(account);

  // The account storage is available in principle, so the opt-in will be shown.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  ASSERT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));

  // Opt in.
  OptInToAccountStorage(&pref_service_, &sync_service_);
  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  ASSERT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kAccountStore);

  // Now enable Sync-the-feature. This should effectively turn *off* the account
  // storage again (since with Sync, there's only a single combined storage),
  // even though the opt-in wasn't actually cleared.
  SetSyncStateFeatureActive(account);
  ASSERT_TRUE(sync_service_.IsSyncFeatureEnabled());
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilTest, LocalSyncDisablesAccountStorage) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in local-sync mode.
  // In local-sync mode, there might or might not be an account. Set one for
  // this test, so that all other conditions for using the account-scoped
  // storage are fulfilled.
  SetSyncStateTransportActive(account);
  sync_service_.SetLocalSyncEnabled(true);

  // The account-scoped storage should be unavailable.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Even if the user is opted in (e.g. from a previous browser run, before
  // local-sync was enabled), the account-scoped storage should remain
  // unavailable.
  OptInToAccountStorage(&pref_service_, &sync_service_);
  // The user is *not* considered opted in (even though the corresponding pref
  // is set) since the account storage is completely unavailable.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilTest, OptOutClearsStorePreference) {
  base::HistogramTester histogram_tester;

  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in transport mode.
  SetSyncStateTransportActive(account);

  // Opt in and set default store to profile.
  OptInToAccountStorage(&pref_service_, &sync_service_);
  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  SetDefaultPasswordStore(&pref_service_, &sync_service_,
                          PasswordForm::Store::kProfileStore);

  // Opt out.
  OptOutOfAccountStorageAndClearSettings(&pref_service_, &sync_service_);

  // The default store pref should have been erased.
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // The change to the profile store above should have been recorded. Clearing
  // the pref does not get recorded in this histogram!
  histogram_tester.ExpectUniqueSample("PasswordManager.DefaultPasswordStoreSet",
                                      PasswordForm::Store::kProfileStore, 1);
}

TEST_F(PasswordManagerFeaturesUtilTest, OptInOutHistograms) {
  base::HistogramTester histogram_tester;

  CoreAccountInfo first_account;
  first_account.email = "first@account.com";
  first_account.gaia = "first";
  first_account.account_id = CoreAccountId::FromGaiaId(first_account.gaia);

  CoreAccountInfo second_account;
  second_account.email = "second@account.com";
  second_account.gaia = "second";
  second_account.account_id = CoreAccountId::FromGaiaId(second_account.gaia);

  // Opt in with the first account.
  SetSyncStateTransportActive(first_account);
  OptInToAccountStorage(&pref_service_, &sync_service_);
  // There is now 1 opt-in.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn", 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn", 1, 1);

  // Opt in with the second account.
  SetSyncStateTransportActive(second_account);
  OptInToAccountStorage(&pref_service_, &sync_service_);
  // There are now 2 opt-ins.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn", 2);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn", 2, 1);

  // Out out of the second account again.
  OptOutOfAccountStorageAndClearSettings(&pref_service_, &sync_service_);
  // The OptedIn histogram is unchanged.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn", 2);
  // There is now an OptedOut sample; there is 1 opt-in left.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptOut", 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptOut", 1, 1);

  // Clear all remaining opt-ins (which is just one).
  ClearAccountStorageSettingsForAllUsers(&pref_service_);
  // The OptedIn/OptedOut histograms are unchanged.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptIn", 2);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.NumOptedInAccountsAfterOptOut", 1);
  // There was 1 remaining opt-in that was cleared.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStorage.ClearedOptInForAllAccounts", 1, 1);
}

TEST_F(PasswordManagerFeaturesUtilTest,
       MovePasswordToAccountStoreOfferedCount) {
  // Set up a user signed-in, not syncing and not opted-in.
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  SetSyncStateTransportActive(account);
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  EXPECT_EQ(
      0, GetMoveOfferedToNonOptedInUserCount(&pref_service_, &sync_service_));
  RecordMoveOfferedToNonOptedInUser(&pref_service_, &sync_service_);
  EXPECT_EQ(
      1, GetMoveOfferedToNonOptedInUserCount(&pref_service_, &sync_service_));
  RecordMoveOfferedToNonOptedInUser(&pref_service_, &sync_service_);
  EXPECT_EQ(
      2, GetMoveOfferedToNonOptedInUserCount(&pref_service_, &sync_service_));
}

}  // namespace features_util
}  // namespace password_manager
