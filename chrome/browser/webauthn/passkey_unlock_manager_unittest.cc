// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include <string>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "device/fido/public/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace webauthn {

namespace {

constexpr char kTestAccount[] = "usertest@gmail.com";

sync_pb::WebauthnCredentialSpecifics CreatePasskey() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_rp_id("abc1.com");
  passkey.set_user_id({1, 2, 3, 4});
  passkey.set_user_name("passkey_username");
  passkey.set_user_display_name("passkey_display_name");
  return passkey;
}

class MockPasskeyUnlockManagerObserver : public PasskeyUnlockManager::Observer {
 public:
  MOCK_METHOD(void, OnPasskeyErrorUiStateChanged, (), (override));
  MOCK_METHOD(void, OnPasskeyUnlockManagerShuttingDown, (), (override));
  MOCK_METHOD(void, OnPasskeyUnlockManagerIsReady, (), (override));
};

using ::testing::_;

enum EnclaveManagerStatus { kEnclaveReady, kEnclaveNotReady };
enum PasskeyModelStatus { kPasskeyModelReady, kPasskeyModelNotReady };
using GpmPinStatus = EnclaveManager::GpmPinAvailability;

class PasskeyUnlockManagerTest : public testing::Test {
 protected:
  void ConfigureProfileAndSyncService(
      EnclaveManagerStatus enclave_manager_status,
      PasskeyModelStatus passkey_model_status,
      GpmPinStatus gpm_pin_status) {
    test_enclave_manager_ = CreateMockEnclaveManager(
        /*is_enclave_manager_loaded=*/true,
        /*is_enclave_manager_ready=*/enclave_manager_status == kEnclaveReady,
        gpm_pin_status);
    test_passkey_model_ =
        CreateMockPasskeyModel(passkey_model_status == kPasskeyModelReady);
    test_sync_service_ = CreateTestSyncService();
    passkey_unlock_manager_ = std::make_unique<PasskeyUnlockManager>(
        test_enclave_manager_.get(), test_passkey_model_.get(),
        test_sync_service_.get());

    CoreAccountInfo account_info;
    account_info.email = kTestAccount;
    account_info.gaia = GaiaId("gaia");
    account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
    test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin,
                                     account_info);
    test_sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/true,
        /*types=*/{});
  }

  std::unique_ptr<EnclaveManagerInterface> CreateMockEnclaveManager(
      bool is_enclave_manager_loaded,
      bool is_enclave_manager_ready,
      GpmPinStatus gpm_pin_status) {
    std::unique_ptr<MockEnclaveManager> enclave_manager_mock =
        std::make_unique<MockEnclaveManager>();
    ON_CALL(*enclave_manager_mock, IsLoaded())
        .WillByDefault(testing::Return(is_enclave_manager_loaded));
    ON_CALL(*enclave_manager_mock, IsReady())
        .WillByDefault(testing::Return(is_enclave_manager_ready));
    ON_CALL(*enclave_manager_mock, CheckGpmPinAvailability(_))
        .WillByDefault(
            [gpm_pin_status](
                EnclaveManager::GpmPinAvailabilityCallback callback) {
              std::move(callback).Run(gpm_pin_status);
            });
    EXPECT_CALL(*enclave_manager_mock, CheckGpmPinAvailability(_));
    return enclave_manager_mock;
  }

  std::unique_ptr<PasskeyModel> CreateMockPasskeyModel(
      bool is_passkey_model_ready) {
    std::unique_ptr<webauthn::TestPasskeyModel> test_passkey_model =
        std::make_unique<webauthn::TestPasskeyModel>();
    test_passkey_model->SetReady(is_passkey_model_ready);
    return test_passkey_model;
  }

  std::unique_ptr<syncer::SyncService> CreateTestSyncService() {
    return std::make_unique<syncer::TestSyncService>();
  }

  void TearDown() override { passkey_unlock_manager_.reset(); }

  PasskeyUnlockManager* passkey_unlock_manager() {
    return passkey_unlock_manager_.get();
  }

  TestPasskeyModel* passkey_model() {
    return static_cast<TestPasskeyModel*>(test_passkey_model_.get());
  }

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(test_sync_service_.get());
  }

  void DisableUVKeySupport() {
    fake_provider_.emplace<crypto::ScopedNullUserVerifyingKeyProvider>();
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void EnableUiExperimentArm(std::string arm_name) {
    feature_params_[device::kPasskeyUnlockErrorUiExperimentArm.name] = arm_name;
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        device::kPasskeyUnlockErrorUi, feature_params_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_{device::kPasskeyUnlockManager};
  std::map<std::string, std::string> feature_params_;
  std::unique_ptr<EnclaveManagerInterface> test_enclave_manager_;
  std::unique_ptr<PasskeyModel> test_passkey_model_;
  std::unique_ptr<syncer::SyncService> test_sync_service_;
  std::unique_ptr<PasskeyUnlockManager> passkey_unlock_manager_;
  std::variant<crypto::ScopedFakeUserVerifyingKeyProvider,
               crypto::ScopedNullUserVerifyingKeyProvider,
               crypto::ScopedFailingUserVerifyingKeyProvider>
      fake_provider_;
};

struct PasskeyUnlockManagerErrorUiTestParams {
  std::string name;
  // The error UI should be shown only if the enclave manager is not ready.
  EnclaveManagerStatus enclave_status;
  // The error UI should be shown if there is a way to unlock passkeys via GPM
  // PIN or system UV.
  GpmPinStatus gpm_pin_status;
  bool uv_keys_supported;
  // Disallowing sync should cause the error UI to be hidden.
  bool sync_allowed;
  // The error UI should not be shown when trusted vault key is required because
  // that error has a higher priority.
  bool trusted_vault_key_required;
  // The error UI should not be shown when trusted vault recoverability is
  // degraded because that error has a higher priority.
  bool trusted_vault_recoverability_degraded;
  // Stopping passkeys sync should cause the error UI to be hidden.
  bool passkeys_sync_allowed;
  bool should_display_error_ui;
};

class PasskeyUnlockManagerErrorUiTest
    : public PasskeyUnlockManagerTest,
      public testing::WithParamInterface<
          PasskeyUnlockManagerErrorUiTestParams> {};

TEST_P(PasskeyUnlockManagerErrorUiTest, ShouldDisplayErrorUi) {
  const auto& param = GetParam();
  if (!param.uv_keys_supported) {
    DisableUVKeySupport();
  }
  ConfigureProfileAndSyncService(param.enclave_status, kPasskeyModelReady,
                                 param.gpm_pin_status);
  test_sync_service()->SetAllowedByEnterprisePolicy(param.sync_allowed);
  test_sync_service()->GetUserSettings()->SetTrustedVaultKeyRequired(
      param.trusted_vault_key_required);
  test_sync_service()->GetUserSettings()->SetTrustedVaultRecoverabilityDegraded(
      param.trusted_vault_recoverability_degraded);
  if (!param.passkeys_sync_allowed) {
    test_sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kPreferences});
  }
  test_sync_service()->FireStateChanged();
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  if (param.should_display_error_ui) {
    EXPECT_CALL(observer, OnPasskeyErrorUiStateChanged());
  }

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());

  EXPECT_EQ(passkey_unlock_manager()->ShouldDisplayErrorUi(),
            param.should_display_error_ui);
}

const PasskeyUnlockManagerErrorUiTestParams kErrorUiTestParams[] = {
    {"ShownWithPasskeysAndActiveSync", EnclaveManagerStatus::kEnclaveNotReady,
     GpmPinStatus::kGpmPinUnset,
     /*uv_keys_supported=*/true,
     /*sync_allowed=*/true,
     /*trusted_vault_key_required=*/false,
     /*trusted_vault_recoverability_degraded=*/false,
     /*passkeys_sync_allowed=*/true,
     /*should_display_error_ui=*/true},
    {"NotShownWithPasskeysAndActiveSyncWithEnclaveReady",
     EnclaveManagerStatus::kEnclaveReady, GpmPinStatus::kGpmPinUnset,
     /*uv_keys_supported=*/true,
     /*sync_allowed=*/true,
     /*trusted_vault_key_required=*/false,
     /*trusted_vault_recoverability_degraded=*/false,
     /*passkeys_sync_allowed=*/true,
     /*should_display_error_ui=*/false},
    {"ErrorUiHiddenWhenSyncDisallowed", EnclaveManagerStatus::kEnclaveNotReady,
     GpmPinStatus::kGpmPinUnset,
     /*uv_keys_supported=*/true,
     /*sync_allowed=*/false,
     /*trusted_vault_key_required=*/false,
     /*trusted_vault_recoverability_degraded=*/false,
     /*passkeys_sync_allowed=*/true,
     /*should_display_error_ui=*/false},
    {"ErrorUiHiddenWhenTrustedVaultKeyRequired",
     EnclaveManagerStatus::kEnclaveNotReady, GpmPinStatus::kGpmPinUnset,
     /*uv_keys_supported=*/true,
     /*sync_allowed=*/true,
     /*trusted_vault_key_required=*/true,
     /*trusted_vault_recoverability_degraded=*/false,
     /*passkeys_sync_allowed=*/true,
     /*should_display_error_ui=*/false},
    {"ErrorUiHiddenWhenTrustedVaultRecoverabilityDegraded",
     EnclaveManagerStatus::kEnclaveNotReady, GpmPinStatus::kGpmPinUnset,
     /*uv_keys_supported=*/true,
     /*sync_allowed=*/true,
     /*trusted_vault_key_required=*/false,
     /*trusted_vault_recoverability_degraded=*/true,
     /*passkeys_sync_allowed=*/true,
     /*should_display_error_ui=*/false},
    {"ErrorUiHiddenWhenPasskeysNotSynced",
     EnclaveManagerStatus::kEnclaveNotReady, GpmPinStatus::kGpmPinUnset,
     /*uv_keys_supported=*/true,
     /*sync_allowed=*/true,
     /*trusted_vault_key_required=*/false,
     /*trusted_vault_recoverability_degraded=*/false,
     /*passkeys_sync_allowed=*/false,
     /*should_display_error_ui=*/false},

// On Chrome OS, AreUserVerifyingKeysSupported always returns true, thus this
// tests cannot establish the preconditions.
#if !BUILDFLAG(IS_CHROMEOS)
    {"HiddenWithoutUVKeysWithoutGpmPin", EnclaveManagerStatus::kEnclaveNotReady,
     GpmPinStatus::kGpmPinUnset,
     /*uv_keys_supported=*/false,
     /*sync_allowed=*/true,
     /*trusted_vault_key_required=*/false,
     /*trusted_vault_recoverability_degraded=*/false,
     /*passkeys_sync_allowed=*/true,
     /*should_display_error_ui=*/false},
    {"VisibleWithoutUVKeysWithGpmPin", EnclaveManagerStatus::kEnclaveNotReady,
     GpmPinStatus::kGpmPinSetAndUsable,
     /*uv_keys_supported=*/false,
     /*sync_allowed=*/true,
     /*trusted_vault_key_required=*/false,
     /*trusted_vault_recoverability_degraded=*/false,
     /*passkeys_sync_allowed=*/true,
     /*should_display_error_ui=*/true},
#endif
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PasskeyUnlockManagerErrorUiTest,
    testing::ValuesIn(kErrorUiTestParams),
    [](const testing::TestParamInfo<PasskeyUnlockManagerErrorUiTestParams>&
           info) { return info.param.name; });

struct PasskeyUnlockManagerHistogramTestParams {
  std::string name;
  // The error UI should be shown only if the enclave manager is not ready.
  EnclaveManagerStatus enclave_status;
  // The passkey model is used for checking if the user has passkeys.
  PasskeyModelStatus passkey_model_status;
  // The error UI should be shown if there is a way to unlock passkeys, for
  // example using the GPM PIN.
  GpmPinStatus gpm_pin_status;
  // The number of passkeys to add to the passkey model.
  int num_passkeys_to_add = 0;
  // If true, the passkey model used for testing will become ready, simulating
  // that the model has finished loading state from disk and is ready to sync.
  // Only when passkey model is ready, the passkey count histogram is logged.
  bool trigger_passkey_model_ready = false;
  bool expected_passkey_readiness;
};

class PasskeyUnlockManagerHistogramTest
    : public PasskeyUnlockManagerTest,
      public testing::WithParamInterface<
          PasskeyUnlockManagerHistogramTestParams> {};

TEST_P(PasskeyUnlockManagerHistogramTest, LogsHistograms) {
  const auto& param = GetParam();
  ConfigureProfileAndSyncService(
      param.enclave_status, param.passkey_model_status, param.gpm_pin_status);

  for (int i = 0; i < param.num_passkeys_to_add; ++i) {
    passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  }

  base::HistogramTester histogram_tester;

  passkey_model()->SetReady(param.trigger_passkey_model_ready);

  AdvanceClock(base::Seconds(31));

  histogram_tester.ExpectUniqueSample("WebAuthentication.PasskeyCount",
                                      param.num_passkeys_to_add, 1);
  histogram_tester.ExpectBucketCount("WebAuthentication.PasskeyReadiness",
                                     param.expected_passkey_readiness, 1);
  histogram_tester.ExpectUniqueSample("WebAuthentication.GpmPinStatus",
                                      param.gpm_pin_status, 1);
}

const PasskeyUnlockManagerHistogramTestParams kHistogramTestParams[] = {
    {"PasskeyCount_NoPasskeys", EnclaveManagerStatus::kEnclaveNotReady,
     PasskeyModelStatus::kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
     /*num_passkeys_to_add=*/0,
     /*trigger_passkey_model_ready=*/false,
     /*expected_passkey_readiness=*/false},
    {"PasskeyCount_WithPasskeys", EnclaveManagerStatus::kEnclaveNotReady,
     PasskeyModelStatus::kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
     /*num_passkeys_to_add=*/1,
     /*trigger_passkey_model_ready=*/false,
     /*expected_passkey_readiness=*/false},
    {"PasskeyReadiness_Ready", EnclaveManagerStatus::kEnclaveReady,
     PasskeyModelStatus::kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
     /*num_passkeys_to_add=*/0,
     /*trigger_passkey_model_ready=*/false,
     /*expected_passkey_readiness=*/true},
    {"PasskeyReadiness_Locked", EnclaveManagerStatus::kEnclaveNotReady,
     PasskeyModelStatus::kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
     /*num_passkeys_to_add=*/0,
     /*trigger_passkey_model_ready=*/false,
     /*expected_passkey_readiness=*/false},
    {"PasskeyCount_ModelBecomesReady", EnclaveManagerStatus::kEnclaveNotReady,
     PasskeyModelStatus::kPasskeyModelNotReady, GpmPinStatus::kGpmPinUnset,
     /*num_passkeys_to_add=*/1,
     /*trigger_passkey_model_ready=*/true,
     // The passkey readiness was already recorded as not ready at the time of
     // startup, so if the model becomes ready later it doesn't get recorded
     // again.
     /*expected_passkey_readiness=*/false},
    {"GpmPinStatus_Unset", EnclaveManagerStatus::kEnclaveNotReady,
     PasskeyModelStatus::kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
     /*num_passkeys_to_add=*/0,
     /*trigger_passkey_model_ready=*/false,
     /*expected_passkey_readiness=*/false},
    {"GpmPinStatus_SetAndUsable", EnclaveManagerStatus::kEnclaveNotReady,
     PasskeyModelStatus::kPasskeyModelReady, GpmPinStatus::kGpmPinSetAndUsable,
     /*num_passkeys_to_add=*/0,
     /*trigger_passkey_model_ready=*/false,
     /*expected_passkey_readiness=*/false},
    {"GpmPinStatus_SetAndNotUsable", EnclaveManagerStatus::kEnclaveNotReady,
     PasskeyModelStatus::kPasskeyModelReady,
     GpmPinStatus::kGpmPinSetButNotUsable,
     /*num_passkeys_to_add=*/0,
     /*trigger_passkey_model_ready=*/false,
     /*expected_passkey_readiness=*/false},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PasskeyUnlockManagerHistogramTest,
    testing::ValuesIn(kHistogramTestParams),
    [](const testing::TestParamInfo<PasskeyUnlockManagerHistogramTestParams>&
           info) { return info.param.name; });

TEST_F(PasskeyUnlockManagerTest, IsCreated) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

TEST_F(PasskeyUnlockManagerTest, NotifyOnPasskeysChangedWhenPasskeyAdded) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  EXPECT_CALL(observer, OnPasskeyErrorUiStateChanged());
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);
}

TEST_F(PasskeyUnlockManagerTest, TextLablesForDifferentUiExperimentArms) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  EnableUiExperimentArm("text_with_verify_wording");
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY));
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_VERIFY));
  EXPECT_EQ(
      passkey_unlock_manager()->GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_VERIFY));

  EnableUiExperimentArm("text_with_get_wording");
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_GET));
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_GET));
  EXPECT_EQ(
      passkey_unlock_manager()->GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_GET));

  EnableUiExperimentArm("text_with_unlock_wording");
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_UNLOCK));
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_UNLOCK));
  EXPECT_EQ(
      passkey_unlock_manager()->GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_UNLOCK));
}

}  // namespace

}  // namespace webauthn
