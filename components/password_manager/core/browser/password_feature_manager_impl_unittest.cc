// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_feature_manager_impl.h"

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordFeatureManagerImplTest : public ::testing::Test {
 public:
  PasswordFeatureManagerImplTest()
      : password_feature_manager_(&pref_service_, &sync_service_) {
    pref_service_.registry()->RegisterDictionaryPref(
        password_manager::prefs::kAccountStoragePerAccountSettings);
    account_.email = "account@gmail.com";
    account_.gaia = "account";
    account_.account_id = CoreAccountId::FromGaiaId(account_.gaia);
  }

  ~PasswordFeatureManagerImplTest() override = default;

 protected:
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
  password_manager::PasswordFeatureManagerImpl password_feature_manager_;
  CoreAccountInfo account_;
};

TEST_F(PasswordFeatureManagerImplTest, GenerationEnabledIfUserIsOptedIn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  sync_service_.SetAccountInfo(account_);
  sync_service_.SetHasSyncConsent(false);
  sync_service_.SetDisableReasons({});
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);

  password_feature_manager_.OptInToAccountStorage();

  ASSERT_EQ(
      password_manager_util::GetPasswordSyncState(&sync_service_),
      password_manager::SyncState::kAccountPasswordsActiveNormalEncryption);

  EXPECT_TRUE(password_feature_manager_.IsGenerationEnabled());
}

TEST_F(PasswordFeatureManagerImplTest,
       GenerationEnabledIfUserEligibleForAccountStorageOptIn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  sync_service_.SetAccountInfo(account_);
  sync_service_.SetHasSyncConsent(false);
  sync_service_.SetDisableReasons({});
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_.SetActiveDataTypes({});

  ASSERT_EQ(password_manager_util::GetPasswordSyncState(&sync_service_),
            password_manager::SyncState::kNotSyncing);
  // The user must be eligible for account storage opt in now.
  ASSERT_TRUE(password_feature_manager_.ShouldShowAccountStorageOptIn());

  EXPECT_TRUE(password_feature_manager_.IsGenerationEnabled());
}

TEST_F(PasswordFeatureManagerImplTest,
       GenerationDisabledIfUserNotEligibleForAccountStorageOptIn) {
  // Setup one example of user not eligible for opt in: signed in but with
  // feature flag disabled.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  sync_service_.SetAccountInfo(account_);
  sync_service_.SetHasSyncConsent(false);
  sync_service_.SetDisableReasons({});
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_.SetActiveDataTypes({});

  ASSERT_EQ(password_manager_util::GetPasswordSyncState(&sync_service_),
            password_manager::SyncState::kNotSyncing);
  // The user must not be eligible for account storage opt in now.
  ASSERT_FALSE(password_feature_manager_.ShouldShowAccountStorageOptIn());

  EXPECT_FALSE(password_feature_manager_.IsGenerationEnabled());
}

TEST_F(PasswordFeatureManagerImplTest,
       RequirementsForAutomatedPasswordChangeMetForSyncingUser) {
  sync_service_.SetAccountInfo(account_);
  sync_service_.SetHasSyncConsent(true);
  sync_service_.SetDisableReasons({});
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_.SetActiveDataTypes(syncer::ModelTypeSet(syncer::PASSWORDS));

  ASSERT_EQ(password_manager_util::GetPasswordSyncState(&sync_service_),
            password_manager::SyncState::kSyncingNormalEncryption);

  EXPECT_TRUE(password_feature_manager_
                  .AreRequirementsForAutomatedPasswordChangeFulfilled());
}

TEST_F(PasswordFeatureManagerImplTest,
       RequirementsForAutomatedPasswordChangeNotMetForNonSyncingUser) {
  sync_service_.SetAccountInfo(account_);
  sync_service_.SetHasSyncConsent(false);
  sync_service_.SetDisableReasons(
      {syncer::SyncService::DisableReason::DISABLE_REASON_USER_CHOICE});
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_.SetActiveDataTypes({});

  ASSERT_EQ(password_manager_util::GetPasswordSyncState(&sync_service_),
            password_manager::SyncState::kNotSyncing);

  EXPECT_FALSE(password_feature_manager_
                   .AreRequirementsForAutomatedPasswordChangeFulfilled());
}

TEST_F(PasswordFeatureManagerImplTest,
       RequirementsForAutomatedPasswordChangeNotMetForAccountStoreUser) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  sync_service_.SetAccountInfo(account_);
  sync_service_.SetHasSyncConsent(false);
  sync_service_.SetDisableReasons(
      {syncer::SyncService::DisableReason::DISABLE_REASON_USER_CHOICE});
  sync_service_.SetTransportState(syncer::SyncService::TransportState::ACTIVE);

  password_feature_manager_.OptInToAccountStorage();

  ASSERT_EQ(
      password_manager_util::GetPasswordSyncState(&sync_service_),
      password_manager::SyncState::kAccountPasswordsActiveNormalEncryption);

  EXPECT_FALSE(password_feature_manager_
                   .AreRequirementsForAutomatedPasswordChangeFulfilled());
}
