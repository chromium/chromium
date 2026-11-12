// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/rand_util.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "device/fido/public/features.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
using ::testing::Eq;
using ::testing::Field;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

enum EnclaveManagerStatus { kEnclaveReady, kEnclaveNotReady };
enum PasskeyModelStatus { kPasskeyModelReady, kPasskeyModelNotReady };
using GpmPinStatus = EnclaveManager::GpmPinAvailability;
using AccountState =
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;

AccountState MakeAccountState(
    AccountState::State state = AccountState::State::kRecoverable) {
  AccountState account_state;
  account_state.state = state;
  return account_state;
}

class PasskeyUnlockManagerTest : public testing::Test {
 protected:
  void ConfigureProfileAndSyncService(
      EnclaveManagerStatus enclave_manager_status,
      PasskeyModelStatus passkey_model_status,
      GpmPinStatus gpm_pin_status,
      syncer::PassphraseType passphrase_type =
          syncer::PassphraseType::kKeystorePassphrase,
      bool trusted_vault_key_required = false,
      syncer::DataTypeSet inactive_sync_data_types = {}) {
    test_enclave_manager_ = CreateMockEnclaveManager(
        /*is_enclave_manager_loaded=*/true,
        /*is_enclave_manager_ready=*/enclave_manager_status == kEnclaveReady,
        gpm_pin_status);
    test_passkey_model_ =
        CreateMockPasskeyModel(passkey_model_status == kPasskeyModelReady);
    test_sync_service_ = CreateTestSyncService();
    test_sync_service()->GetUserSettings()->SetPassphraseType(passphrase_type);
    test_sync_service()->GetUserSettings()->SetTrustedVaultKeyRequired(
        trusted_vault_key_required);
    test_sync_service()->SetFailedDataTypes(inactive_sync_data_types);
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
        .WillByDefault(Return(is_enclave_manager_loaded));
    ON_CALL(*enclave_manager_mock, IsReady())
        .WillByDefault(Return(is_enclave_manager_ready));
    ON_CALL(*enclave_manager_mock, DownloadAccountState(_))
        .WillByDefault(
            [](EnclaveManagerInterface::DownloadAccountStateCallback callback) {
              std::move(callback).Run(MakeAccountState(), GaiaId("gaia"));
              return std::make_unique<
                  trusted_vault::TrustedVaultConnection::Request>();
            });
    ON_CALL(*enclave_manager_mock, CheckGpmPinAvailability(_))
        .WillByDefault(Return(gpm_pin_status));
    if (is_enclave_manager_loaded) {
      // The security domain state is downloaded once, as soon as the enclave
      // manager has loaded.
      EXPECT_CALL(*enclave_manager_mock, DownloadAccountState(_)).Times(1);
    }
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

  MockEnclaveManager* enclave_manager_mock() {
    return static_cast<MockEnclaveManager*>(test_enclave_manager_.get());
  }

  void PrepareDependencies(bool is_enclave_manager_loaded,
                           EnclaveManagerStatus enclave_manager_status,
                           PasskeyModelStatus passkey_model_status,
                           GpmPinStatus gpm_pin_status) {
    test_enclave_manager_ = CreateMockEnclaveManager(
        is_enclave_manager_loaded, enclave_manager_status == kEnclaveReady,
        gpm_pin_status);
    test_passkey_model_ =
        CreateMockPasskeyModel(passkey_model_status == kPasskeyModelReady);
    test_sync_service_ = CreateTestSyncService();
    test_sync_service()->GetUserSettings()->SetPassphraseType(
        syncer::PassphraseType::kKeystorePassphrase);
    test_sync_service()->GetUserSettings()->SetTrustedVaultKeyRequired(false);

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

  void CreatePasskeyUnlockManager() {
    passkey_unlock_manager_ = std::make_unique<PasskeyUnlockManager>(
        test_enclave_manager_.get(), test_passkey_model_.get(),
        test_sync_service_.get());
  }

  // Creates the `PasskeyUnlockManager` before the enclave manager has loaded.
  // Requires `PrepareDependencies()` with `is_enclave_manager_loaded` set to
  // false. `LoadEnclaveManager()` then simulates the delayed load.
  void CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads() {
    EXPECT_CALL(*enclave_manager_mock(), LoadAfterDelay(_, _))
        .WillOnce([this](base::TimeDelta, base::OnceClosure on_loaded) {
          on_enclave_manager_loaded_ = std::move(on_loaded);
        });
    CreatePasskeyUnlockManager();
  }

  void LoadEnclaveManager() {
    ON_CALL(*enclave_manager_mock(), IsLoaded()).WillByDefault(Return(true));
    std::move(on_enclave_manager_loaded_).Run();
    NotifyEnclaveStateUpdated();
  }

  void RunDelayedEnclaveManagerLoadCallback() {
    std::move(on_enclave_manager_loaded_).Run();
  }

  // Expects `DownloadAccountState()` to be called once and captures its
  // callback into `download_callback`, which must outlive the expected call.
  void ExpectAccountStateDownload(
      EnclaveManagerInterface::DownloadAccountStateCallback&
          download_callback) {
    EXPECT_CALL(*enclave_manager_mock(), DownloadAccountState(_))
        .Times(1)
        .WillOnce(MoveArgAndReturn<0>(
            &download_callback,
            std::make_unique<
                trusted_vault::TrustedVaultConnection::Request>()));
  }

  void NotifyEnclaveStateUpdated() {
    static_cast<EnclaveManager::Observer*>(passkey_unlock_manager())
        ->OnStateUpdated();
  }

  // Fires state-change notifications from Sync, `PasskeyModel`, and
  // `EnclaveManager` to verify stage transitions and idempotence.
  void TriggerDependencyUpdates() {
    test_sync_service()->FireStateChanged();
    passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
    NotifyEnclaveStateUpdated();
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

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<EnclaveManagerInterface> test_enclave_manager_;
  std::unique_ptr<PasskeyModel> test_passkey_model_;
  std::unique_ptr<syncer::SyncService> test_sync_service_;
  std::unique_ptr<PasskeyUnlockManager> passkey_unlock_manager_;
  // Simulates the enclave manager finishing its delayed load.
  base::OnceClosure on_enclave_manager_loaded_;
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
      public WithParamInterface<PasskeyUnlockManagerErrorUiTestParams> {};

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
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
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
    ValuesIn(kErrorUiTestParams),
    [](const TestParamInfo<PasskeyUnlockManagerErrorUiTestParams>& info) {
      return info.param.name;
    });

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
      public WithParamInterface<PasskeyUnlockManagerHistogramTestParams> {};

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
    ValuesIn(kHistogramTestParams),
    [](const TestParamInfo<PasskeyUnlockManagerHistogramTestParams>& info) {
      return info.param.name;
    });

TEST_F(PasskeyUnlockManagerTest, IsCreated) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

TEST_F(PasskeyUnlockManagerTest, NotifyOnPasskeysChangedWhenPasskeyAdded) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  EXPECT_CALL(observer, OnPasskeyErrorUiStateChanged());
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);
}

TEST_F(PasskeyUnlockManagerTest,
       LogsPasswordReadinessHistogram_PasswordsAreReady) {
  base::test::ScopedFeatureList scoped_feature_list{
      password_manager::features::kRecordPasswordReadiness};

  base::HistogramTester histogram_tester;
  ConfigureProfileAndSyncService(
      kEnclaveNotReady, kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
      syncer::PassphraseType::kTrustedVaultPassphrase,
      /*trusted_vault_key_required=*/false);

  AdvanceClock(base::Seconds(31));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TrustedVaultPasswordReadiness",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyUnlockManagerTest,
       LogsPasswordReadinessHistogram_PasswordsAreNotReady) {
  base::test::ScopedFeatureList scoped_feature_list{
      password_manager::features::kRecordPasswordReadiness};

  base::HistogramTester histogram_tester;
  ConfigureProfileAndSyncService(
      kEnclaveNotReady, kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
      syncer::PassphraseType::kTrustedVaultPassphrase,
      /*trusted_vault_key_required=*/true);

  AdvanceClock(base::Seconds(31));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TrustedVaultPasswordReadiness",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyUnlockManagerTest,
       DoesNotLogPasswordReadinessHistogramWhenNotTrustedVault) {
  base::test::ScopedFeatureList scoped_feature_list{
      password_manager::features::kRecordPasswordReadiness};

  base::HistogramTester histogram_tester;
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset,
                                 syncer::PassphraseType::kKeystorePassphrase);

  AdvanceClock(base::Seconds(31));

  histogram_tester.ExpectTotalCount(
      "PasswordManager.TrustedVaultPasswordReadiness", 0);
}

TEST_F(PasskeyUnlockManagerTest,
       DoesNotLogPasswordReadinessHistogramUntilPasswordsAreActive) {
  base::test::ScopedFeatureList scoped_feature_list{
      password_manager::features::kRecordPasswordReadiness};

  base::HistogramTester histogram_tester;
  ConfigureProfileAndSyncService(
      kEnclaveNotReady, kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
      syncer::PassphraseType::kTrustedVaultPassphrase,
      /*trusted_vault_key_required=*/false,
      /*inactive_sync_data_types=*/{syncer::PASSWORDS});

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.TrustedVaultPasswordReadiness", 0);

  // This makes `PASSWORDS` an active sync data type.
  test_sync_service()->SetFailedDataTypes({});
  test_sync_service()->FireStateChanged();

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TrustedVaultPasswordReadiness",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(PasskeyUnlockManagerTest, DoesNotReportPasskeysLockedWhenFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      device::kPasskeyUnlockIdentityErrorUi);
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());
}

TEST_F(PasskeyUnlockManagerTest,
       SetPasskeysLockedReflectsLockAndUnlockableState) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockIdentityErrorUi};
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  // Reports passkeys locked when passkeys are present, locked, and unlockable.
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());

  // Remains true when Sync reports an actionable error or when passkey sync
  // becomes inactive, because the state pushed to
  // `SyncService::SetPasskeysLocked()` does not depend on Sync state
  // (`SyncService` gates on `WEBAUTHN_CREDENTIAL` itself).
  test_sync_service()->GetUserSettings()->SetTrustedVaultRecoverabilityDegraded(
      true);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_sync_service()->passkeys_locked());

  test_sync_service()->SetFailedDataTypes({syncer::WEBAUTHN_CREDENTIAL});
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Clears passkeys locked when the enclave becomes ready.
  ON_CALL(*enclave_manager_mock(), IsReady()).WillByDefault(Return(true));
  static_cast<EnclaveManager::Observer*>(passkey_unlock_manager())
      ->OnStateUpdated();
  EXPECT_FALSE(test_sync_service()->passkeys_locked());
}

TEST_F(PasskeyUnlockManagerTest,
       SetPasskeysLockedTrueWhenPasskeySyncInactiveAndSyncErrorPresent) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockIdentityErrorUi};
  ConfigureProfileAndSyncService(
      kEnclaveNotReady, kPasskeyModelReady, GpmPinStatus::kGpmPinUnset,
      syncer::PassphraseType::kTrustedVaultPassphrase,
      /*trusted_vault_key_required=*/true,
      /*inactive_sync_data_types=*/{syncer::WEBAUTHN_CREDENTIAL});

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PasskeyUnlockManagerTest, DoesNotReportPasskeysLockedWhenNotUnlockable) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockIdentityErrorUi};
  DisableUVKeySupport();
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PasskeyUnlockManagerTest,
       ReportedPasskeysLockedIgnoresSyncStateChanges) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockIdentityErrorUi};
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Updates that keep the Sync-facing state true (including Sync state changes
  // that flip `ShouldDisplayErrorUi()` to false and back to true, or change
  // Sync's actionable error) keep `passkeys_locked()` true.
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  test_sync_service()->GetUserSettings()->SetTrustedVaultRecoverabilityDegraded(
      true);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  test_sync_service()->SetFailedDataTypes({syncer::WEBAUTHN_CREDENTIAL});
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  test_sync_service()->SetFailedDataTypes({});
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Becoming ready clears `passkeys_locked()`.
  ON_CALL(*enclave_manager_mock(), IsReady()).WillByDefault(Return(true));
  static_cast<EnclaveManager::Observer*>(passkey_unlock_manager())
      ->OnStateUpdated();
  EXPECT_FALSE(test_sync_service()->passkeys_locked());
}

TEST_F(PasskeyUnlockManagerTest,
       PasskeysLockedStateStaysTrueWhenSyncReportsPasskeysLocked) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockIdentityErrorUi};
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  ASSERT_EQ(test_sync_service()->GetUserActionableError(),
            syncer::SyncService::UserActionableError::kPasskeysLocked);
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Subsequent `OnStateChanged()` notifications while Sync still reports
  // `kPasskeysLocked` must keep `passkeys_locked()` and
  // `ShouldDisplayErrorUi()` true.
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest, ShutdownKeepsReportedPasskeysLocked) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockIdentityErrorUi};
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(test_sync_service()->passkeys_locked());

  static_cast<KeyedService*>(passkey_unlock_manager())->Shutdown();
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
}

TEST_F(PasskeyUnlockManagerTest,
       AccountStateDownloadedOnceAfterEnclaveManagerLoads) {
  PrepareDependencies(/*is_enclave_manager_loaded=*/false, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinUnset);
  // Notifications before the enclave manager loads must not start a download.
  EXPECT_CALL(*enclave_manager_mock(), DownloadAccountState(_)).Times(0);
  CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads();
  TriggerDependencyUpdates();
  Mock::VerifyAndClearExpectations(enclave_manager_mock());

  EnclaveManagerInterface::DownloadAccountStateCallback download_callback;
  ExpectAccountStateDownload(download_callback);
  LoadEnclaveManager();
  ASSERT_FALSE(download_callback.is_null());

  // State updates while the download is in flight or after it completes must
  // not start another download.
  TriggerDependencyUpdates();
  std::move(download_callback).Run(MakeAccountState(), GaiaId("gaia"));
  TriggerDependencyUpdates();
}

TEST_F(PasskeyUnlockManagerTest,
       AccountStateDownloadedWhenEnclaveManagerLoadsEarly) {
  PrepareDependencies(/*is_enclave_manager_loaded=*/false, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinUnset);
  EXPECT_CALL(*enclave_manager_mock(), DownloadAccountState(_)).Times(0);
  CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads();
  Mock::VerifyAndClearExpectations(enclave_manager_mock());

  // Another caller loads the enclave manager before the `LoadAfterDelay()`
  // callback runs, notifying observers via `OnStateUpdated()`.
  EXPECT_CALL(*enclave_manager_mock(), DownloadAccountState(_)).Times(1);
  ON_CALL(*enclave_manager_mock(), IsLoaded()).WillByDefault(Return(true));
  NotifyEnclaveStateUpdated();
  Mock::VerifyAndClearExpectations(enclave_manager_mock());

  // Running the `LoadAfterDelay()` callback afterwards does not download again.
  EXPECT_CALL(*enclave_manager_mock(), DownloadAccountState(_)).Times(0);
  RunDelayedEnclaveManagerLoadCallback();
}

TEST_F(PasskeyUnlockManagerTest, GpmPinStatusRecordedAfterEnclaveManagerLoads) {
  base::HistogramTester histogram_tester;
  PrepareDependencies(/*is_enclave_manager_loaded=*/false, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinSetButNotUsable);
  CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads();

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectTotalCount("WebAuthentication.GpmPinStatus", 0);

  // The status is recorded 30 seconds after the security domain state has been
  // downloaded, which happens once the enclave manager has loaded.
  LoadEnclaveManager();
  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectUniqueSample("WebAuthentication.GpmPinStatus",
                                      GpmPinStatus::kGpmPinSetButNotUsable, 1);
}

TEST_F(PasskeyUnlockManagerTest,
       ErrorUiAndObserversSuppressedUntilDownloadFinishes) {
  base::test::ScopedFeatureList scoped_feature_list;
#if BUILDFLAG(IS_MAC)
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{device::kPasskeyUnlockIdentityErrorUi},
      /*disabled_features=*/{device::kPasskeyUnlockICloudRecovery});
#else
  scoped_feature_list.InitAndEnableFeature(
      device::kPasskeyUnlockIdentityErrorUi);
#endif
  PrepareDependencies(/*is_enclave_manager_loaded=*/false, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinUnset);
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EnclaveManagerInterface::DownloadAccountStateCallback download_callback;
  ExpectAccountStateDownload(download_callback);
  CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads();
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  // Even with system UV available and passkeys locked once the enclave manager
  // loads, the error UI, observer notifications, and Sync lock state are
  // suppressed until the download finishes.
  LoadEnclaveManager();
  ASSERT_FALSE(download_callback.is_null());
  TriggerDependencyUpdates();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  EXPECT_CALL(observer, OnPasskeyErrorUiStateChanged()).Times(1);
  std::move(download_callback).Run(MakeAccountState(), GaiaId("gaia"));
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
}

// On ChromeOS, system UV is always available, so whether passkeys can be
// unlocked doesn't depend on the GPM PIN.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PasskeyUnlockManagerTest,
       ErrorUiWaitsForGpmPinFromDownloadedAccountState) {
  DisableUVKeySupport();
  PrepareDependencies(/*is_enclave_manager_loaded=*/false, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinSetAndUsable);
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EnclaveManagerInterface::DownloadAccountStateCallback download_callback;
  ExpectAccountStateDownload(download_callback);
  CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads();
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  // Passkeys are locked once the enclave manager has loaded. Without system UV,
  // whether they can be unlocked depends on the GPM PIN, which isn't known
  // until the security domain state has been downloaded.
  LoadEnclaveManager();
  ASSERT_FALSE(download_callback.is_null());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  EXPECT_CALL(*enclave_manager_mock(),
              CheckGpmPinAvailability(Field(&AccountState::state,
                                            AccountState::State::kRecoverable)))
      .WillOnce(Return(GpmPinStatus::kGpmPinSetAndUsable));
  EXPECT_CALL(observer, OnPasskeyErrorUiStateChanged());
  std::move(download_callback).Run(MakeAccountState(), GaiaId("gaia"));
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
TEST_F(PasskeyUnlockManagerTest, ICloudRecoveryNotCalledWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      device::kPasskeyUnlockICloudRecovery);
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

struct ICloudRecoverySkippedTestCase {
  std::string name;
  bool identity_error_ui_enabled = true;
  EnclaveManagerStatus enclave_status = kEnclaveNotReady;
  bool has_passkeys = true;
  bool passkey_sync_active = true;
  bool sync_error_present = false;
};

class PasskeyUnlockManagerICloudRecoverySkippedTest
    : public PasskeyUnlockManagerTest,
      public WithParamInterface<ICloudRecoverySkippedTestCase> {};

TEST_P(PasskeyUnlockManagerICloudRecoverySkippedTest,
       DoesNotStartICloudRecovery) {
  const ICloudRecoverySkippedTestCase& param = GetParam();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureStates(
      {{device::kPasskeyUnlockICloudRecovery, true},
       {device::kPasskeyUnlockIdentityErrorUi,
        param.identity_error_ui_enabled}});
  PrepareDependencies(/*is_enclave_manager_loaded=*/true, param.enclave_status,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinSetAndUsable);
  if (param.has_passkeys) {
    passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  }
  if (!param.passkey_sync_active) {
    test_sync_service()->SetFailedDataTypes({syncer::WEBAUTHN_CREDENTIAL});
  }
  if (param.sync_error_present) {
    test_sync_service()
        ->GetUserSettings()
        ->SetTrustedVaultRecoverabilityDegraded(true);
  }

  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);
  CreatePasskeyUnlockManager();
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  test_sync_service()->FireStateChanged();
  NotifyEnclaveStateUpdated();
  // While awaiting iCloud Keychain recovery (or when passkeys aren't locked),
  // neither the error UI nor Sync's passkeys-locked state is reported.
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());
}

const ICloudRecoverySkippedTestCase kICloudRecoverySkippedTestCases[] = {
    {.name = "WithoutPasskeys", .has_passkeys = false},
    {.name = "WhenPasskeySyncInactive", .passkey_sync_active = false},
    {.name = "WhenEnclaveReady", .enclave_status = kEnclaveReady},
    {.name = "WhenSyncErrorPresentAndIdentityErrorUiDisabled",
     .identity_error_ui_enabled = false,
     .sync_error_present = true},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PasskeyUnlockManagerICloudRecoverySkippedTest,
    ValuesIn(kICloudRecoverySkippedTestCases),
    [](const TestParamInfo<ICloudRecoverySkippedTestCase>& info) {
      return info.param.name;
    });

TEST_F(PasskeyUnlockManagerTest,
       ICloudRecoveryLifecycleWhenPasskeysLockedAtDownload) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{device::kPasskeyUnlockICloudRecovery,
                            device::kPasskeyUnlockIdentityErrorUi},
      /*disabled_features=*/{});
  PrepareDependencies(/*is_enclave_manager_loaded=*/false, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinSetAndUsable);
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EnclaveManagerInterface::DownloadAccountStateCallback download_callback;
  ExpectAccountStateDownload(download_callback);
  CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads();
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  // While the security domain download is in flight, recovery waits and UI /
  // Sync notifications remain suppressed.
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);
  LoadEnclaveManager();
  ASSERT_FALSE(download_callback.is_null());
  TriggerDependencyUpdates();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());
  Mock::VerifyAndClearExpectations(enclave_manager_mock());

  // Once downloaded, recovery starts once with the downloaded state and
  // `GaiaId`; updates while recovery is in flight keep UI / Sync suppressed and
  // do not re-trigger recovery.
  EnclaveManagerInterface::Callback recovery_callback;
  EXPECT_CALL(*enclave_manager_mock(),
              RecoverWithICloudKeychain(
                  Optional(Field(&AccountState::state,
                                 AccountState::State::kRecoverable)),
                  Optional(GaiaId("gaia2")), _))
      .Times(1)
      .WillOnce(MoveArg<2>(&recovery_callback));
  std::move(download_callback).Run(MakeAccountState(), GaiaId("gaia2"));
  ASSERT_FALSE(recovery_callback.is_null());
  Mock::VerifyAndClearExpectations(enclave_manager_mock());
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);

  TriggerDependencyUpdates();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  // When recovery fails, the error UI and Sync lock state are reported once,
  // and later updates do not re-trigger recovery.
  EXPECT_CALL(observer, OnPasskeyErrorUiStateChanged()).Times(1);
  std::move(recovery_callback).Run(false);
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());

  TriggerDependencyUpdates();
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
}

TEST_F(PasskeyUnlockManagerTest,
       ICloudRecoveryStartsWhenEnclaveManagerLoadsEarly) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockICloudRecovery};
  PrepareDependencies(/*is_enclave_manager_loaded=*/false, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinSetAndUsable);
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);
  CreatePasskeyUnlockManagerBeforeEnclaveManagerLoads();
  Mock::VerifyAndClearExpectations(enclave_manager_mock());

  // Another caller loads the enclave manager before `LoadAfterDelay()` fires;
  // recovery starts right after the download with the downloaded state.
  EXPECT_CALL(*enclave_manager_mock(), DownloadAccountState(_)).Times(1);
  EXPECT_CALL(*enclave_manager_mock(),
              RecoverWithICloudKeychain(
                  Optional(Field(&AccountState::state,
                                 AccountState::State::kRecoverable)),
                  Optional(GaiaId("gaia")), _));
  ON_CALL(*enclave_manager_mock(), IsLoaded()).WillByDefault(Return(true));
  NotifyEnclaveStateUpdated();
  Mock::VerifyAndClearExpectations(enclave_manager_mock());

  EXPECT_CALL(*enclave_manager_mock(), DownloadAccountState(_)).Times(0);
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);
  RunDelayedEnclaveManagerLoadCallback();
}

TEST_F(PasskeyUnlockManagerTest,
       ICloudRecoveryRunsOnceWhenPasskeysSyncInLater) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{device::kPasskeyUnlockICloudRecovery,
                            device::kPasskeyUnlockIdentityErrorUi},
      /*disabled_features=*/{});
  PrepareDependencies(/*is_enclave_manager_loaded=*/true, kEnclaveNotReady,
                      kPasskeyModelNotReady, GpmPinStatus::kGpmPinSetAndUsable);

  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);
  CreatePasskeyUnlockManager();
  Mock::VerifyAndClearExpectations(enclave_manager_mock());

  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  // Passkeys weren't locked at download time, so recovery starts once with
  // `std::nullopt` when passkeys sync in later.
  EnclaveManagerInterface::Callback recovery_callback;
  EXPECT_CALL(*enclave_manager_mock(),
              RecoverWithICloudKeychain(Eq(std::nullopt), Eq(std::nullopt), _))
      .Times(1)
      .WillOnce(MoveArg<2>(&recovery_callback));

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  passkey_model()->SetReady(true);
  ASSERT_FALSE(recovery_callback.is_null());
  Mock::VerifyAndClearExpectations(enclave_manager_mock());
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .Times(0);
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  TriggerDependencyUpdates();
  EXPECT_CALL(observer, OnPasskeyErrorUiStateChanged()).Times(1);
  std::move(recovery_callback).Run(false);
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
  EXPECT_TRUE(test_sync_service()->passkeys_locked());

  TriggerDependencyUpdates();
}

TEST_F(PasskeyUnlockManagerTest,
       ICloudRecoveryPassesNulloptWhenDownloadFailed) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockICloudRecovery};
  PrepareDependencies(/*is_enclave_manager_loaded=*/true, kEnclaveNotReady,
                      kPasskeyModelReady, GpmPinStatus::kGpmPinUnset);
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ON_CALL(*enclave_manager_mock(), DownloadAccountState(_))
      .WillByDefault(
          [](EnclaveManagerInterface::DownloadAccountStateCallback callback) {
            std::move(callback).Run(
                MakeAccountState(AccountState::State::kError), GaiaId("gaia"));
            return std::make_unique<
                trusted_vault::TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*enclave_manager_mock(),
              CheckGpmPinAvailability(
                  Field(&AccountState::state, AccountState::State::kError)))
      .WillOnce(Return(GpmPinStatus::kGpmPinUnset));
  EXPECT_CALL(*enclave_manager_mock(),
              RecoverWithICloudKeychain(Eq(std::nullopt), Eq(std::nullopt), _));
  CreatePasskeyUnlockManager();
}

TEST_F(PasskeyUnlockManagerTest, ICloudRecoverySucceedsHidesErrorUi) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockICloudRecovery};
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  EnclaveManagerInterface::Callback recovery_callback;
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .WillOnce(MoveArg<2>(&recovery_callback));

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_FALSE(recovery_callback.is_null());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  ON_CALL(*enclave_manager_mock(), IsReady()).WillByDefault(Return(true));
  std::move(recovery_callback).Run(true);
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest,
       ICloudRecoveryFailureAndSystemIsNotUnlockable) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kPasskeyUnlockICloudRecovery};
  DisableUVKeySupport();
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  StrictMock<MockPasskeyUnlockManagerObserver> observer;
  base::ScopedObservation<PasskeyUnlockManager, PasskeyUnlockManager::Observer>
      observation(&observer);
  observation.Observe(passkey_unlock_manager());

  EnclaveManagerInterface::Callback recovery_callback;
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .WillOnce(MoveArg<2>(&recovery_callback));

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_FALSE(recovery_callback.is_null());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  std::move(recovery_callback).Run(false);
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest,
       ICloudRecoveryRunsWhenRecoverabilityDegradedAndReportsLockedOnFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{device::kPasskeyUnlockICloudRecovery,
                            device::kPasskeyUnlockIdentityErrorUi},
      /*disabled_features=*/{});
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  test_sync_service()->GetUserSettings()->SetTrustedVaultRecoverabilityDegraded(
      true);
  test_sync_service()->FireStateChanged();
  ASSERT_EQ(test_sync_service()->GetUserActionableError(),
            syncer::SyncService::UserActionableError::
                kTrustedVaultRecoverabilityDegradedForPasswords);

  EnclaveManagerInterface::Callback recovery_callback;
  EXPECT_CALL(*enclave_manager_mock(), RecoverWithICloudKeychain(_, _, _))
      .WillOnce(MoveArg<2>(&recovery_callback));

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_FALSE(recovery_callback.is_null());
  EXPECT_FALSE(test_sync_service()->passkeys_locked());

  std::move(recovery_callback).Run(false);
  EXPECT_TRUE(test_sync_service()->passkeys_locked());
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

}  // namespace webauthn
