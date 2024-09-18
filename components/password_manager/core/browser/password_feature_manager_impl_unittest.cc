// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_feature_manager_impl.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
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
    pref_service_.registry()->RegisterBooleanPref(
        ::prefs::kExplicitBrowserSignin, false);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

    account_.email = "account@gmail.com";
    account_.gaia = "account";
    account_.account_id = CoreAccountId::FromGaiaId(account_.gaia);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
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

TEST_F(PasswordFeatureManagerImplTest,
       GenerationEnabledIfNonSyncingAndUsingAccountStorage) {
  base::test::ScopedFeatureList feature_list(
      syncer::kEnablePasswordsAccountStorageForNonSyncingUsers);
#if BUILDFLAG(IS_ANDROID)
  pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
#endif  // BUILDFLAG(IS_ANDROID)

  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_);

  ASSERT_EQ(
      password_manager::sync_util::GetPasswordSyncState(&sync_service_),
      password_manager::sync_util::SyncState::kActiveWithNormalEncryption);
  ASSERT_FALSE(
      password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
          &sync_service_));

  EXPECT_TRUE(password_feature_manager_.IsGenerationEnabled());
}

TEST_F(PasswordFeatureManagerImplTest, GenerationEnabledIfSyncing) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_);

  ASSERT_EQ(
      password_manager::sync_util::GetPasswordSyncState(&sync_service_),
      password_manager::sync_util::SyncState::kActiveWithNormalEncryption);
  ASSERT_TRUE(
      password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
          &sync_service_));

  EXPECT_TRUE(password_feature_manager_.IsGenerationEnabled());
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

class PasswordFeatureManagerImplExplicitSigninParamTest
    : public base::test::WithFeatureOverride,
      public PasswordFeatureManagerImplTest {
 public:
  PasswordFeatureManagerImplExplicitSigninParamTest()
      : base::test::WithFeatureOverride(
            ::switches::kExplicitBrowserSigninUIOnDesktop) {
    // `::prefs::kExplicitBrowserSignin` should only be set if
    // `switches::kExplicitBrowserSigninUIOnDesktop` is enabled.
    pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin,
                             IsExplicitSignin());
  }

  bool IsExplicitSignin() const {
    return ::switches::IsExplicitBrowserSigninUIOnDesktopEnabled();
  }
};

// Desktop users can be offered to opt in to account storage if eligible. One
// such offer is triggered from the generation entry point, so
// IsGenerationEnabled() must return true in that state.
// When signin is explicit, account storage is ON by default, and password
// generation no longer triggers an optin.
TEST_P(PasswordFeatureManagerImplExplicitSigninParamTest,
       GenerationEnabledIfUserEligibleForAccountStorageOptIn) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_);
  // The user hasn't opted in to account storage yet.
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  ASSERT_EQ(password_manager::sync_util::GetPasswordSyncState(&sync_service_),
            password_manager::sync_util::SyncState::kNotActive);

  // If signin is implicit, the user must be eligible for account storage opt in
  // now. When signin is explicit, account storage is ON by default, and
  // password generation no longer triggers an optin.
  ASSERT_EQ(password_feature_manager_.ShouldShowAccountStorageOptIn(),
            !IsExplicitSignin());

  EXPECT_EQ(password_feature_manager_.IsGenerationEnabled(),
            !IsExplicitSignin());
}

// When signin is explicit, account storage remains disabled in auth errors.
TEST_P(PasswordFeatureManagerImplExplicitSigninParamTest,
       OptedInIfSigninPaused) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_);
  sync_service_.SetPersistentAuthError();

  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_EQ(password_manager::sync_util::GetPasswordSyncState(&sync_service_),
            password_manager::sync_util::SyncState::kNotActive);
  EXPECT_FALSE(password_feature_manager_.IsOptedInForAccountStorage());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PasswordFeatureManagerImplExplicitSigninParamTest);

#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// On Android, for certain versions of GMS Core, signed-in users have a single
// (profile) PasswordStore that successfully talks to the account GmsCore
// backend. Such users should be able to generate passwords, so
// IsGenerationEnabled() should return true. If the account backend is not
// available, generation is disabled, but that is decided on a different layer.
TEST_F(PasswordFeatureManagerImplTest,
       GenerationEnabledEvenIfCannotCreateAccountStore) {
  pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));

  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_);

  ASSERT_EQ(
      password_manager::sync_util::GetPasswordSyncState(&sync_service_),
      password_manager::sync_util::SyncState::kActiveWithNormalEncryption);

  EXPECT_TRUE(password_feature_manager_.IsGenerationEnabled());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PasswordFeatureManagerImplTest, GenerationDisabledIfSignedOut) {
  sync_service_.SetSignedOut();

  ASSERT_EQ(password_manager::sync_util::GetPasswordSyncState(&sync_service_),
            password_manager::sync_util::SyncState::kNotActive);

  EXPECT_FALSE(password_feature_manager_.IsGenerationEnabled());
}

TEST_F(PasswordFeatureManagerImplTest, GenerationDisabledIfSyncPaused) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_);
  sync_service_.SetPersistentAuthError();

  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_EQ(password_manager::sync_util::GetPasswordSyncState(&sync_service_),
            password_manager::sync_util::SyncState::kNotActive);

  EXPECT_FALSE(password_feature_manager_.IsGenerationEnabled());
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordFeatureManagerImplTest, ShouldChangeDefaultPasswordStore) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_);

  password_feature_manager_.SetDefaultPasswordStore(
      password_manager::PasswordForm::Store::kProfileStore);
  EXPECT_TRUE(password_feature_manager_.ShouldChangeDefaultPasswordStore());
}

TEST_F(PasswordFeatureManagerImplTest, ShouldNotChangeDefaultPasswordStore) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_);

  password_feature_manager_.SetDefaultPasswordStore(
      password_manager::PasswordForm::Store::kAccountStore);
  EXPECT_FALSE(password_feature_manager_.ShouldChangeDefaultPasswordStore());
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

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
  SCOPED_TRACE(test_case.description);
  base::test::ScopedFeatureList feature_list;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (test_case.feature_flag) {
    feature_list.InitAndEnableFeature(
        password_manager::features::kBiometricsAuthForPwdFill);
  } else {
    feature_list.InitAndDisableFeature(
        password_manager::features::kBiometricsAuthForPwdFill);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)  || BUILDFLAG(IS_CHROMEOS_ASH)
