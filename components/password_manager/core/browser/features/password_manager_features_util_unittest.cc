// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager::features_util {

class PasswordManagerFeaturesUtilTestBase : public testing::Test {
 public:
  PasswordManagerFeaturesUtilTestBase() {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kAccountStoragePerAccountSettings);

    // Passwords starts enabled default in TestSyncUserSettings, so disable it
    // to mimic production behavior.
    sync_service_.GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPasswords, false);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
};

#if BUILDFLAG(IS_ANDROID)
// Test fixture where the account-scoped password storage is *disabled* for both
// syncing and non-syncing users, i.e. CanCreateAccountStore() is false. Android
// is the only platform still supporting that.
class PasswordManagerFeaturesUtilWithoutAccountStorageTest
    : public PasswordManagerFeaturesUtilTestBase {
 public:
  PasswordManagerFeaturesUtilWithoutAccountStorageTest() {
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  }
};
#endif  // BUIDLFLAG(IS_ANDROID)

// Test fixture where account storage is enabled (via flag) for signed-in
// non-syncing users and disabled for syncing users.
class PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest
    : public PasswordManagerFeaturesUtilTestBase {
 public:
  PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kEnablePasswordsAccountStorageForNonSyncingUsers},
        /*disabled_features=*/{
            syncer::kEnablePasswordsAccountStorageForSyncingUsers});
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
#endif  //  BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        ::prefs::kExplicitBrowserSignin, false);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test fixture where account storage is enabled (via flag) for syncing users
// and disabled for non-syncing users.
class PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest
    : public PasswordManagerFeaturesUtilTestBase {
 public:
  PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kEnablePasswordsAccountStorageForSyncingUsers},
        /*disabled_features=*/{
            syncer::kEnablePasswordsAccountStorageForNonSyncingUsers});
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
#endif  //  BUILDFLAG(IS_ANDROID)
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilWithoutAccountStorageTest,
       AccountStorageOptIn) {
  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = "foo";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // SyncService is running in transport mode with |account| and the opt-in
  // bit is even present.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);

  // Since the account storage is disabled, the profile store should be the
  // the default.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Same if the user is syncing.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Same if the user is signed out.
  sync_service_.SetSignedOut();
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       AccountStorageOptIn) {
  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = "foo";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  sync_service_.SetSignedOut();

  // Initially the user is not signed in, so everything is off/local.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Now let SyncService run in transport mode with |account|.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  // By default, the user is not opted in, but eligible.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_NE(switches::IsExplicitBrowserSigninUIOnDesktopEnabled(),
            ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Opt in!
  OptInToAccountStorage(&pref_service_, &sync_service_);
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  // Now the default is saving to the account.
  EXPECT_TRUE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kAccountStore);

  // TODO(b/40943570): Remove this section since setting default password store
  // will not be possible. Change the default store to the profile one.
  SetDefaultPasswordStore(&pref_service_, &sync_service_,
                          PasswordForm::Store::kProfileStore);
  EXPECT_TRUE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Sign out. Now the settings should have reasonable default values (not opted
  // in, save to profile store).
  sync_service_.SetSignedOut();
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       AccountStorageKeepSettingsOnlyForUsers) {
  CoreAccountInfo first_account;
  first_account.email = "first@account.com";
  first_account.gaia = "first";
  first_account.account_id = CoreAccountId::FromGaiaId(first_account.gaia);

  CoreAccountInfo second_account;
  second_account.email = "second@account.com";
  second_account.gaia = "second";
  second_account.account_id = CoreAccountId::FromGaiaId(second_account.gaia);

  // Let SyncService run in transport mode with |first_account|, opt in and
  // set the profile store as default.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, first_account);
  OptInToAccountStorage(&pref_service_, &sync_service_);
  SetDefaultPasswordStore(&pref_service_, &sync_service_,
                          PasswordForm::Store::kProfileStore);
  ASSERT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Switch to |second_account| and do the same.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, second_account);
  OptInToAccountStorage(&pref_service_, &sync_service_);
  SetDefaultPasswordStore(&pref_service_, &sync_service_,
                          PasswordForm::Store::kProfileStore);
  ASSERT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  // Sign out. The store settings still exist, but don't apply anymore.
  sync_service_.SetSignedOut();
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // Keep the settings only for |first_account| (and some unknown other user).
  KeepAccountStorageSettingsOnlyForUsers(&pref_service_,
                                         {first_account.gaia, "other_gaia_id"});

  // The first account should still have kProfileStore as the default store,
  // but not the second.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, first_account);
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, second_account);
  EXPECT_NE(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       SyncSuppressesAccountStorageOptIn) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // Initially, the user is signed in but doesn't have Sync-the-feature enabled,
  // so the SyncService is running in transport mode.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  // In this state, the user could opt in to the account storage.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  ASSERT_NE(switches::IsExplicitBrowserSigninUIOnDesktopEnabled(),
            ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));

  // Now the user enables Sync-the-feature.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  ASSERT_TRUE(sync_service_.IsSyncFeatureEnabled());

  // Now the account-storage opt-in should *not* be available anymore.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       NoOptInOfferedIfExplicitSigninUIEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
      /*disabled_features=*/{});
  pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, true);
  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = "foo";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);
  OptOutOfAccountStorage(&pref_service_, &sync_service_);
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // The opt-in should not be shown, kExplicitBrowserSigninUIOnDesktop is
  // enabled. The toggle is always shown.
  EXPECT_FALSE(AreAccountStorageOptInPromosAllowed());
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_TRUE(
      ShouldShowAccountStorageSettingToggle(&pref_service_, &sync_service_));

  pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, false);

  // Explicit and implicit sign-ins are treated alike.
  EXPECT_FALSE(AreAccountStorageOptInPromosAllowed());
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_TRUE(
      ShouldShowAccountStorageSettingToggle(&pref_service_, &sync_service_));
}

#else
TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       AccountStorageOptInOnMobile) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // Initial state: Not signed in.
  sync_service_.SetSignedOut();

  // Without a signed-in user, there can be no opt-in.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // Sign in and enable Sync-transport.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  // Now the user should be considered opted-in.
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // Disable the Passwords data type, which corresponds to the user opting out.
  syncer::UserSelectableTypeSet selected_types =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  selected_types.Remove(syncer::UserSelectableType::kPasswords);
  sync_service_.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    selected_types);

  // The user should not be considered opted-in anymore.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       SyncDisablesAccountStorage) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // The SyncService is running in transport mode.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // The account storage is available in principle, so the opt-in will be shown.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  ASSERT_NE(switches::IsExplicitBrowserSigninUIOnDesktopEnabled(),
            ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  ASSERT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));

  // Opt in.
  OptInToAccountStorage(&pref_service_, &sync_service_);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  ASSERT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  ASSERT_TRUE(ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
#else
  EXPECT_FALSE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
#endif
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kAccountStore);

  // Now enable Sync-the-feature. This should effectively turn *off* the account
  // storage again (since with Sync, there's only a single combined storage).
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  ASSERT_TRUE(sync_service_.IsSyncFeatureEnabled());
  // On desktop, the opt-in pref wasn't actually cleared, but
  // IsOptedInForAccountStorage() must return false because the user is syncing.
  // On mobile, since no explicit opt-in exists, the (implicit) opt-in is gone.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       LocalSyncDisablesAccountStorage) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in local-sync mode.
  // In local-sync mode, there might or might not be an account. Set one for
  // this test, so that all other conditions for using the account-scoped
  // storage are fulfilled.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);
  sync_service_.SetLocalSyncEnabled(true);

  // The account-scoped storage should be unavailable.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service_, &sync_service_));
  EXPECT_FALSE(
      ShouldShowAccountStorageBubbleUi(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       OptOutAndClearSettingsClearsStorePreference) {
  base::HistogramTester histogram_tester;

  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in transport mode.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

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

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       OptOutSetsProfileStorePreference) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in transport mode.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  // Opt in.
  OptInToAccountStorage(&pref_service_, &sync_service_);
  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  // Opt out.
  OptOutOfAccountStorage(&pref_service_, &sync_service_);

  // The default store pref should set to profile store.
  EXPECT_TRUE(IsDefaultPasswordStoreSet(&pref_service_, &sync_service_));
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       MigrateOptInPrefToSyncSelectedTypes) {
  syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  CoreAccountInfo account1;
  account1.gaia = "gaia1";
  auto gaia1_hash = signin::GaiaIdHash::FromGaiaId(account1.gaia);
  CoreAccountInfo account2;
  account2.gaia = "gaia2";
  auto gaia2_hash = signin::GaiaIdHash::FromGaiaId(account2.gaia);
  pref_service_.SetDict(
      prefs::kAccountStoragePerAccountSettings,
      base::Value::Dict()
          .Set(gaia1_hash.ToBase64(),
               base::Value::Dict()
                   .Set("opted_in", true)
                   .Set("default_store",
                        static_cast<int>(PasswordForm::Store::kAccountStore)))
          .Set(gaia2_hash.ToBase64(),
               base::Value::Dict()
                   .Set("opted_in", true)
                   .Set("default_store",
                        static_cast<int>(PasswordForm::Store::kProfileStore))));

  MigrateOptInPrefToSyncSelectedTypes(&pref_service_);

  syncer::SyncPrefs sync_prefs(&pref_service_);
  EXPECT_TRUE(sync_prefs.GetSelectedTypesForAccount(gaia1_hash)
                  .Has(syncer::UserSelectableType::kPasswords));
  EXPECT_TRUE(sync_prefs.GetSelectedTypesForAccount(gaia2_hash)
                  .Has(syncer::UserSelectableType::kPasswords));
  EXPECT_FALSE(
      sync_prefs
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId("other"))
          .Has(syncer::UserSelectableType::kPasswords));

  // Verify the default store settings are unnaffected.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account1);
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kAccountStore);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account2);
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       MigrateDeclinedSaveOptInToExplicitOptOut) {
  // Using the Uno Flag because it will automatically turn on account password
  // storage if the default store has not been set to kProfileStore.
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kExplicitBrowserSigninUIOnDesktop};
  syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, true);

  CoreAccountInfo account1;
  account1.gaia = "gaia1";
  auto gaia_with_account_store_hash =
      signin::GaiaIdHash::FromGaiaId(account1.gaia);
  CoreAccountInfo account2;
  account2.gaia = "gaia2";
  auto gaia_with_profile_store_hash =
      signin::GaiaIdHash::FromGaiaId(account2.gaia);
  pref_service_.SetDict(
      prefs::kAccountStoragePerAccountSettings,
      base::Value::Dict()
          .Set(gaia_with_account_store_hash.ToBase64(),
               base::Value::Dict().Set(
                   "default_store",
                   static_cast<int>(PasswordForm::Store::kAccountStore)))
          .Set(gaia_with_profile_store_hash.ToBase64(),
               base::Value::Dict().Set(
                   "default_store",
                   static_cast<int>(PasswordForm::Store::kProfileStore))));

  syncer::SyncPrefs sync_prefs(&pref_service_);
  // Without the migration, passwords storage will be turned on.
  EXPECT_TRUE(
      sync_prefs.GetSelectedTypesForAccount(gaia_with_account_store_hash)
          .Has(syncer::UserSelectableType::kPasswords));
  EXPECT_TRUE(
      sync_prefs.GetSelectedTypesForAccount(gaia_with_profile_store_hash)
          .Has(syncer::UserSelectableType::kPasswords));
  EXPECT_TRUE(
      sync_prefs
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId("other"))
          .Has(syncer::UserSelectableType::kPasswords));

  MigrateDeclinedSaveOptInToExplicitOptOut(&pref_service_);

  EXPECT_TRUE(
      sync_prefs.GetSelectedTypesForAccount(gaia_with_account_store_hash)
          .Has(syncer::UserSelectableType::kPasswords));
  // After the migration, passwords storage will be turned off if the default
  // store was kProfileStore.
  EXPECT_FALSE(
      sync_prefs.GetSelectedTypesForAccount(gaia_with_profile_store_hash)
          .Has(syncer::UserSelectableType::kPasswords));
  EXPECT_TRUE(
      sync_prefs
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId("other"))
          .Has(syncer::UserSelectableType::kPasswords));

  // Verify the default store settings are unaffected.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account1);
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kAccountStore);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account2);
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service_, &sync_service_),
            PasswordForm::Store::kProfileStore);
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest,
       OptedInIfSyncingAndPasswordsSelected) {
  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = "foo";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);

  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest,
       OptedOutIfNonSyncing) {
  sync_service_.SetSignedOut();

  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));

  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = "foo";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);

  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service_, &sync_service_));
}

}  // namespace password_manager::features_util
