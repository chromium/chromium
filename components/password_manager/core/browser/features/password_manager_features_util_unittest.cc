// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
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
namespace {

class PasswordManagerFeaturesUtilTest : public testing::Test {
 public:
  PasswordManagerFeaturesUtilTest() {
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));
#endif
  }

  PasswordManagerFeaturesUtilTest(const PasswordManagerFeaturesUtilTest&) =
      delete;
  PasswordManagerFeaturesUtilTest& operator=(
      const PasswordManagerFeaturesUtilTest&) = delete;

  ~PasswordManagerFeaturesUtilTest() override = default;

 protected:
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
};

TEST_F(PasswordManagerFeaturesUtilTest, IsAccountStorageEnabled_SignedOut) {
  sync_service_.SetSignedOut();

  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageEnabled_SignedInWithLocalSyncEnabled) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetLocalSyncEnabled(true);

  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageEnabled_SignedInWithAuthError) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetPersistentAuthError();

  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageEnabled_SignedInWithPassphraseError) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetPassphraseRequired();

  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageEnabled_SignedInWithTrustedVaultError) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetTrustedVaultKeyRequired(true);

  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageEnabled_SignedInWithPasswordsDataTypeDisabled) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageEnabled_SignedInHealthy) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest, IsAccountStorageEnabled_Syncing) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync);

  EXPECT_EQ(IsAccountStorageEnabled(&pref_service_, &sync_service_),
            BUILDFLAG(IS_ANDROID));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilTest,
       CanCreateAccountStore_LoginDbDeprecationOff) {
  using enum prefs::UseUpmLocalAndSeparateStoresState;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kLoginDbDeprecationAndroid);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_TRUE(CanCreateAccountStore(&pref_service_));
  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  pref_service_.SetInteger(prefs::kPasswordsUseUPMLocalAndSeparateStores,
                           static_cast<int>(kOffAndMigrationPending));

  EXPECT_TRUE(CanCreateAccountStore(&pref_service_));
  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  pref_service_.SetInteger(prefs::kPasswordsUseUPMLocalAndSeparateStores,
                           static_cast<int>(kOff));

  EXPECT_FALSE(CanCreateAccountStore(&pref_service_));
  EXPECT_FALSE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       CanCreateAccountStore_LoginDbDeprecationOn) {
  using enum prefs::UseUpmLocalAndSeparateStoresState;
  base::test::ScopedFeatureList feature_list(
      features::kLoginDbDeprecationAndroid);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_TRUE(CanCreateAccountStore(&pref_service_));
  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  pref_service_.SetInteger(prefs::kPasswordsUseUPMLocalAndSeparateStores,
                           static_cast<int>(kOffAndMigrationPending));

  EXPECT_TRUE(CanCreateAccountStore(&pref_service_));
  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));

  pref_service_.SetInteger(prefs::kPasswordsUseUPMLocalAndSeparateStores,
                           static_cast<int>(kOff));

  EXPECT_TRUE(CanCreateAccountStore(&pref_service_));
  EXPECT_TRUE(IsAccountStorageEnabled(&pref_service_, &sync_service_));
}
#endif

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilTest,
       ShouldShowAccountStorageSettingToggle_SyncToSigninOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_TRUE(
      ShouldShowAccountStorageSettingToggle(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       ShouldShowAccountStorageSettingToggle_SyncToSigninOn) {
  base::test::ScopedFeatureList feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_FALSE(
      ShouldShowAccountStorageSettingToggle(&pref_service_, &sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest, MigrateDefaultProfileStorePref) {
  syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  pref_service_.registry()->RegisterDictionaryPref(
      prefs::kObsoleteAccountStoragePerAccountSettings);
  pref_service_.registry()->RegisterBooleanPref(::prefs::kExplicitBrowserSignin,
                                                false);
  pref_service_.registry()->RegisterBooleanPref(
      ::prefs::kPrefsThemesSearchEnginesAccountStorageEnabled, false);

  // Set up 2 account storage users, with default stores "profile" and
  // "account", respectively.
  GaiaId profile_store_user_gaia("profile");
  GaiaId account_store_user_gaia("account");
  auto profile_store_user_hash =
      signin::GaiaIdHash::FromGaiaId(profile_store_user_gaia);
  auto account_store_user_hash =
      signin::GaiaIdHash::FromGaiaId(account_store_user_gaia);
  syncer::SyncPrefs sync_prefs(&pref_service_);
  sync_prefs.SetSelectedTypeForAccount(syncer::UserSelectableType::kPasswords,
                                       true, profile_store_user_gaia);
  sync_prefs.SetSelectedTypeForAccount(syncer::UserSelectableType::kPasswords,
                                       true, account_store_user_gaia);
  pref_service_.SetDict(
      prefs::kObsoleteAccountStoragePerAccountSettings,
      base::Value::Dict()
          .Set(profile_store_user_hash.ToBase64(),
               base::Value::Dict().Set(
                   kObsoleteAccountStorageDefaultStoreKey,
                   static_cast<int>(PasswordForm::Store::kProfileStore)))
          .Set(account_store_user_hash.ToBase64(),
               base::Value::Dict().Set(
                   kObsoleteAccountStorageDefaultStoreKey,
                   static_cast<int>(PasswordForm::Store::kAccountStore))));

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

}  // namespace
}  // namespace password_manager::features_util
