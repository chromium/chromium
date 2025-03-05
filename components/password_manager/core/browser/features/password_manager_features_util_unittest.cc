// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include "base/feature_list.h"
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
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager::features_util {

class PasswordManagerFeaturesUtilTestBase : public testing::Test {
 public:
  PasswordManagerFeaturesUtilTestBase() {
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kObsoleteAccountStoragePerAccountSettings);

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
    // The UPM state only matters prior to login db deprecation.
    feature_list_.InitAndDisableFeature(
        password_manager::features::kLoginDbDeprecationAndroid);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // BUIDLFLAG(IS_ANDROID)

using LoginDbDeprecated = base::StrongAlias<class LoginDbDeprecatedTag, bool>;

// Test fixture where account storage is enabled (via flag) for signed-in
// non-syncing users and disabled for syncing users.
class PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest
    : public PasswordManagerFeaturesUtilTestBase,
      public testing::WithParamInterface<LoginDbDeprecated> {
 public:
  PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest() {
#if BUILDFLAG(IS_ANDROID)
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{password_manager::features::
                                    kLoginDbDeprecationAndroid},
          /*disabled_features=*/{
              syncer::kEnablePasswordsAccountStorageForSyncingUsers});
      // The UPM status pref shouldn't matter anymore. If the Login Db is
      // deprecated, the account store can theoretically be used.
      pref_service_.registry()->RegisterIntegerPref(
          prefs::kPasswordsUseUPMLocalAndSeparateStores,
          static_cast<int>(password_manager::prefs::
                               UseUpmLocalAndSeparateStoresState::kOff));
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              password_manager::features::kLoginDbDeprecationAndroid,
              syncer::kEnablePasswordsAccountStorageForSyncingUsers});
      pref_service_.registry()->RegisterIntegerPref(
          prefs::kPasswordsUseUPMLocalAndSeparateStores,
          static_cast<int>(
              password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
    }

#endif  //  BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndDisableFeature(
        syncer::kEnablePasswordsAccountStorageForSyncingUsers);
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
    : public PasswordManagerFeaturesUtilTestBase,
      public testing::WithParamInterface<LoginDbDeprecated> {
 public:
  PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest() {
#if BUILDFLAG(IS_ANDROID)
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {password_manager::features::kLoginDbDeprecationAndroid,
           syncer::kEnablePasswordsAccountStorageForSyncingUsers},
          /*disabled_features=*/{});
      // The UPM status pref shouldn't matter anymore. If the Login Db is
      // deprecated, the account store can theoretically be used.
      pref_service_.registry()->RegisterIntegerPref(
          prefs::kPasswordsUseUPMLocalAndSeparateStores,
          static_cast<int>(password_manager::prefs::
                               UseUpmLocalAndSeparateStoresState::kOff));
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {syncer::kEnablePasswordsAccountStorageForSyncingUsers},
          /*disabled_features=*/{
              password_manager::features::kLoginDbDeprecationAndroid,
          });
      pref_service_.registry()->RegisterIntegerPref(
          prefs::kPasswordsUseUPMLocalAndSeparateStores,
          static_cast<int>(
              password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
    }
#else
    feature_list_.InitAndEnableFeature(
        syncer::kEnablePasswordsAccountStorageForSyncingUsers);
#endif  //  BUILDFLAG(IS_ANDROID)
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilWithoutAccountStorageTest,
       AccountStorageDisabled) {
  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = GaiaId("foo");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // SyncService is running in transport mode with |account| and account storage
  // is enabled.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);

  // Account storage should be disabled.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Same if the user is syncing.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Same if the user is signed out.
  sync_service_.SetSignedOut();
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_P(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       EnableAccountStorage) {
  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = GaiaId("foo");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  sync_service_.SetSignedOut();

  // Initially the user is not signed in, so everything is off/local.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Now let SyncService run in transport mode with |account|.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  // By default, the user has account storage disabled, but is eligible.
  // TODO(crbug.com/375024026): Revisit this test.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Enable!
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);
  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Sign out. Now account storage should be off.
  sync_service_.SetSignedOut();
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_P(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       SyncSuppressesAccountStorage) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = GaiaId("name");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // Initially, the user is signed in but doesn't have Sync-the-feature enabled,
  // so the SyncService is running in transport mode.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  // In this state, the user could enable to the account storage.
  ASSERT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Now the user enables Sync-the-feature.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  ASSERT_TRUE(sync_service_.IsSyncFeatureEnabled());

  // Now the account-storage should be disabled.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

#else
TEST_P(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       AccountStorageOnMobile) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = GaiaId("name");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // Initial state: Not signed in.
  sync_service_.SetSignedOut();

  // Without a signed-in user, account storage is disabled.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Sign in and enable Sync-transport.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  // Account storage should be considered enabled.
  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Disable the Passwords data type, which corresponds to disabling account
  // storage.
  syncer::UserSelectableTypeSet selected_types =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  selected_types.Remove(syncer::UserSelectableType::kPasswords);
  sync_service_.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    selected_types);

  // Account storage should be considered disabled.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

TEST_P(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       SyncDisablesAccountStorage) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = GaiaId("name");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  ASSERT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // The SyncService is running in transport mode.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  ASSERT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Enable.
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

  ASSERT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  // Now enable Sync-the-feature. This should effectively turn *off* the account
  // storage again (since with Sync, there's only a single combined storage).
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  ASSERT_TRUE(sync_service_.IsSyncFeatureEnabled());
  // IsAccountStorageEnabled() must return false because the user is syncing.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_P(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       LocalSyncDisablesAccountStorage) {
  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = GaiaId("name");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in local-sync mode.
  // In local-sync mode, there might or might not be an account. Set one for
  // this test, so that all other conditions for using the account-scoped
  // storage are fulfilled.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);
  sync_service_.SetLocalSyncEnabled(true);

  // The account-scoped storage should be unavailable.
  ASSERT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // Even if account storage is enabled (e.g. from a previous browser run,
  // before local-sync was enabled), the account-scoped storage should remain
  // unavailable.
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);
  // Account storage is *disabled* (even though the corresponding pref is set)
  // since local sync is enabled.
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
    testing::Values(LoginDbDeprecated(true), LoginDbDeprecated(false)));
#else
INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
    testing::Values(LoginDbDeprecated(false)));
#endif

TEST_P(PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest,
       AccountStorageEnabledIfSyncingAndPasswordsSelected) {
  CoreAccountInfo account;
  account.email = "foo@account.com";
  account.gaia = GaiaId("foo");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);

  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

#if BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest,
    testing::Values(LoginDbDeprecated(true), LoginDbDeprecated(false)));
#else
INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordManagerFeaturesUtilWithAccountStorageForSyncingUsersTest,
    testing::Values(LoginDbDeprecated(false)));
#endif

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilWithAccountStorageForNonSyncingTest,
       MigrateDefaultProfileStorePref) {
  // Set up 2 account storage users, with default stores "profile" and
  // "account", respectively.
  using password_manager::features_util::kObsoleteAccountStorageDefaultStoreKey;
  GaiaId profile_store_user_gaia("profile");
  GaiaId account_store_user_gaia("account");
  auto profile_store_user_hash =
      signin::GaiaIdHash::FromGaiaId(profile_store_user_gaia);
  auto account_store_user_hash =
      signin::GaiaIdHash::FromGaiaId(account_store_user_gaia);
  syncer::SyncPrefs sync_prefs(&pref_service_);
  sync_prefs.SetSelectedTypeForAccount(syncer::UserSelectableType::kPasswords,
                                       true, profile_store_user_hash);
  sync_prefs.SetSelectedTypeForAccount(syncer::UserSelectableType::kPasswords,
                                       true, account_store_user_hash);
  pref_service_.SetDict(
      password_manager::prefs::kObsoleteAccountStoragePerAccountSettings,
      base::Value::Dict()
          .Set(profile_store_user_hash.ToBase64(),
               base::Value::Dict().Set(
                   kObsoleteAccountStorageDefaultStoreKey,
                   static_cast<int>(
                       password_manager::PasswordForm::Store::kProfileStore)))
          .Set(account_store_user_hash.ToBase64(),
               base::Value::Dict().Set(
                   kObsoleteAccountStorageDefaultStoreKey,
                   static_cast<int>(
                       password_manager::PasswordForm::Store::kAccountStore))));

  // Without the migration, account storage will be on for both accounts upon
  // sign-in.
  EXPECT_TRUE(sync_prefs.GetSelectedTypesForAccount(profile_store_user_gaia)
                  .Has(syncer::UserSelectableType::kPasswords));
  EXPECT_TRUE(sync_prefs.GetSelectedTypesForAccount(account_store_user_gaia)
                  .Has(syncer::UserSelectableType::kPasswords));

  MigrateDefaultProfileStorePref(&pref_service_);

  // After the migration, account storage will be off for the user with profile
  // store as the default.
  EXPECT_FALSE(sync_prefs.GetSelectedTypesForAccount(profile_store_user_gaia)
                   .Has(syncer::UserSelectableType::kPasswords));
  EXPECT_TRUE(sync_prefs.GetSelectedTypesForAccount(account_store_user_gaia)
                  .Has(syncer::UserSelectableType::kPasswords));
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager::features_util
