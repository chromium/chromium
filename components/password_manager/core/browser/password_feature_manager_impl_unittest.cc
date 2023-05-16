// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_feature_manager_impl.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordFeatureManagerImplTest : public ::testing::Test {
 public:
  PasswordFeatureManagerImplTest()
      : password_feature_manager_(&pref_service_,
                                  &pref_service_,
                                  &sync_service_) {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterDictionaryPref(
        password_manager::prefs::kAccountStoragePerAccountSettings);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

    account_.email = "account@gmail.com";
    account_.gaia = "account";
    account_.account_id = CoreAccountId::FromGaiaId(account_.gaia);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kHadBiometricsAvailable, false);
#endif
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

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  password_feature_manager_.OptInToAccountStorage();
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

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

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // Hack: Mark Passwords as not user-selected, so that the TestSyncService will
  // not report it as active.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  ASSERT_EQ(password_manager_util::GetPasswordSyncState(&sync_service_),
            password_manager::SyncState::kNotSyncing);

  // The user must be eligible for account storage opt in now.
  ASSERT_TRUE(password_feature_manager_.ShouldShowAccountStorageOptIn());
#else
  // On Android and iOS, no explicit opt-in exists, so the user is treated as
  // opted-in by default.
  ASSERT_TRUE(password_feature_manager_.IsOptedInForAccountStorage());
  ASSERT_EQ(
      password_manager_util::GetPasswordSyncState(&sync_service_),
      password_manager::SyncState::kAccountPasswordsActiveNormalEncryption);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

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
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  ASSERT_EQ(password_manager_util::GetPasswordSyncState(&sync_service_),
            password_manager::SyncState::kNotSyncing);
  // The user must not be eligible for account storage opt in now.
  ASSERT_FALSE(password_feature_manager_.ShouldShowAccountStorageOptIn());

  EXPECT_FALSE(password_feature_manager_.IsGenerationEnabled());
}

TEST_F(PasswordFeatureManagerImplTest, GenerationDisabledIfSyncPaused) {
  sync_service_.SetAccountInfo(account_);
  sync_service_.SetHasSyncConsent(true);
  sync_service_.SetDisableReasons({});
  sync_service_.SetPersistentAuthError();

  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_EQ(password_manager_util::GetPasswordSyncState(&sync_service_),
            password_manager::SyncState::kNotSyncing);

  EXPECT_FALSE(password_feature_manager_.IsGenerationEnabled());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

struct TestCase {
  const char* description;
  bool had_biometrics;
  bool feature_flag;
  bool pref_value;
};

class PasswordFeatureManagerImplTestBiometricAuthenticationTest
    : public PasswordFeatureManagerImplTest,
      public testing::WithParamInterface<TestCase> {};

TEST_P(PasswordFeatureManagerImplTestBiometricAuthenticationTest,
       IsBiometricAuthenticationBeforeFillingEnabled) {
  TestCase test_case = GetParam();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      password_manager::features::kBiometricAuthenticationForFilling,
      test_case.had_biometrics);

  SCOPED_TRACE(test_case.description);

  pref_service_.SetBoolean(password_manager::prefs::kHadBiometricsAvailable,
                           test_case.feature_flag);
  pref_service_.SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling,
      test_case.pref_value);
  EXPECT_EQ(test_case.had_biometrics && test_case.feature_flag &&
                test_case.pref_value,
            password_feature_manager_
                .IsBiometricAuthenticationBeforeFillingEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordFeatureManagerImplTestBiometricAuthenticationTest,
    ::testing::Values(
        TestCase{
            .description = "Did not have biometric",
            .had_biometrics = false,
            .feature_flag = false,
            .pref_value = false,
        },
        TestCase{
            .description = "Had biometric, but feature disabled",
            .had_biometrics = true,
            .feature_flag = false,
            .pref_value = false,
        },
        TestCase{
            .description = "Had biometric, feature enabled but didn't opt in",
            .had_biometrics = true,
            .feature_flag = true,
            .pref_value = false,
        },
        TestCase{
            .description = "Had biometric, feature enabled, opted in",
            .had_biometrics = true,
            .feature_flag = true,
            .pref_value = true,
        }));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
