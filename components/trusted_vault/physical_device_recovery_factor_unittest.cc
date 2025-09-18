// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/physical_device_recovery_factor.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/test/fake_file_access.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

using testing::_;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace {

const std::vector<uint8_t> kVaultKey = {1, 2, 3};
constexpr int kLastKeyVersion = 1;

MATCHER_P(MatchTrustedVaultKeyAndVersions, expected, "") {
  const auto* trusted_vault_keys =
      std::get_if<std::vector<TrustedVaultKeyAndVersion>>(&arg);
  if (!trusted_vault_keys) {
    *result_listener << "does not hold a vector of TrustedVaultKeyAndVersion";
    return false;
  }
  return testing::ExplainMatchResult(*trusted_vault_keys, expected,
                                     result_listener);
}

MATCHER_P2(TrustedVaultKeyAndVersionEq, expected_key, expected_version, "") {
  const TrustedVaultKeyAndVersion& key_and_version = arg;
  return key_and_version.key == expected_key &&
         key_and_version.version == expected_version;
}

class PhysicalDeviceRecoveryFactorTest : public testing::Test {
 public:
  PhysicalDeviceRecoveryFactorTest() { ResetRecoveryFactor(account_info()); }
  ~PhysicalDeviceRecoveryFactorTest() override = default;

  void ResetRecoveryFactor(const CoreAccountInfo account_info) {
    // Destroy `recovery_factor_`, otherwise it would hold a reference to
    // `storage_` which is destroyed before `recovery_factor_` below.
    recovery_factor_ = nullptr;

    std::unique_ptr<FakeFileAccess> file_access =
        std::make_unique<FakeFileAccess>();
    if (file_access_) {
      // We only want to reset the recovery factor, not the underlying storage.
      file_access->SetStoredLocalTrustedVault(
          file_access_->GetStoredLocalTrustedVault());
    }
    file_access_ = file_access.get();
    storage_ =
        StandaloneTrustedVaultStorage::CreateForTesting(std::move(file_access));
    storage_->ReadDataFromDisk();
    if (storage_->FindUserVault(account_info.gaia) == nullptr) {
      storage_->AddUserVault(account_info.gaia);
      storage_->WriteDataToDisk();
    }

    connection_ =
        std::make_unique<NiceMock<MockTrustedVaultThrottlingConnection>>();

    recovery_factor_ = std::make_unique<PhysicalDeviceRecoveryFactor>(
        SecurityDomainId::kChromeSync, storage_.get(), connection_.get(),
        account_info);
  }

  CoreAccountInfo account_info() {
    CoreAccountInfo account_info;
    account_info.gaia = GaiaId("user");
    return account_info;
  }

  MockTrustedVaultThrottlingConnection* connection() {
    return connection_.get();
  }

  StandaloneTrustedVaultStorage* storage() { return storage_.get(); }

  FakeFileAccess* file_access() { return file_access_; }

  PhysicalDeviceRecoveryFactor* recovery_factor() {
    return recovery_factor_.get();
  }

  // Stores `vault_keys` in storage.
  void StoreKeys(CoreAccountInfo account_info,
                 const std::vector<std::vector<uint8_t>>& vault_keys,
                 int last_vault_key_version) {
    CHECK(!vault_keys.empty());
    trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
        storage_->FindUserVault(account_info.gaia);
    CHECK(per_user_vault);
    per_user_vault->set_last_vault_key_version(last_vault_key_version);
    per_user_vault->set_keys_marked_as_stale_by_consumer(false);
    per_user_vault->clear_vault_key();
    for (const std::vector<uint8_t>& key : vault_keys) {
      AssignBytesToProtoString(
          key, per_user_vault->add_vault_key()->mutable_key_material());
    }
  }

  // Stores `vault_keys` in storage and mimics successful device registration,
  // returns private device key material.
  std::vector<uint8_t> StoreKeysAndMimicDeviceRegistration(
      CoreAccountInfo account_info,
      const std::vector<std::vector<uint8_t>>& vault_keys,
      int last_vault_key_version) {
    StoreKeys(account_info, vault_keys, last_vault_key_version);

    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        device_registration_callback;

    EXPECT_CALL(
        *connection(),
        RegisterAuthenticationFactor(
            Eq(account_info),
            MatchTrustedVaultKeyAndVersions(GetTrustedVaultKeysWithVersions(
                vault_keys, last_vault_key_version)),
            _,
            Eq(AuthenticationFactorTypeAndRegistrationParams(
                LocalPhysicalDevice())),
            _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                const MemberKeysSource& member_keys_source,
                const SecureBoxPublicKey& device_public_key,
                AuthenticationFactorTypeAndRegistrationParams,
                TrustedVaultConnection::RegisterAuthenticationFactorCallback
                    callback) {
              device_registration_callback = std::move(callback);
              // Note: TrustedVaultConnection::Request doesn't support
              // cancellation, so these tests don't cover the contract that
              // caller should store Request object until it's completed or need
              // to be cancelled.
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    recovery_factor_->MaybeRegister(base::DoNothing());
    Mock::VerifyAndClearExpectations(connection());
    EXPECT_FALSE(device_registration_callback.is_null());

    // Pretend that the registration completed successfully.
    std::move(device_registration_callback)
        .Run(TrustedVaultRegistrationStatus::kSuccess,
             /*key_version=*/last_vault_key_version);

    std::string device_private_key_material =
        GetDeviceRegistrationInfo(account_info)->private_key_material();
    return std::vector<uint8_t>(device_private_key_material.begin(),
                                device_private_key_material.end());
  }

  trusted_vault_pb::LocalDeviceRegistrationInfo* GetDeviceRegistrationInfo(
      CoreAccountInfo account_info) {
    trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
        storage_->FindUserVault(account_info.gaia);
    CHECK(per_user_vault);
    return per_user_vault->mutable_local_device_registration_info();
  }

 private:
  std::unique_ptr<StandaloneTrustedVaultStorage> storage_ = nullptr;
  raw_ptr<FakeFileAccess> file_access_ = nullptr;
  std::unique_ptr<NiceMock<MockTrustedVaultThrottlingConnection>> connection_ =
      nullptr;
  std::unique_ptr<PhysicalDeviceRecoveryFactor> recovery_factor_;
};

TEST_F(PhysicalDeviceRecoveryFactorTest, ShouldRegisterDevice) {
  StoreKeys(account_info(), {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info()),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _,
          Eq(AuthenticationFactorTypeAndRegistrationParams(
              LocalPhysicalDevice())),
          _))
      .WillOnce([&](const CoreAccountInfo&, const MemberKeysSource&,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Register the device.
  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_EQ(status, TrustedVaultRecoveryFactorRegistrationStateForUMA::
                        kAttemptingRegistrationWithNewKeyPair);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the registration completed successfully.
  EXPECT_CALL(register_callback,
              Run(TrustedVaultRegistrationStatus::kSuccess, _, _));
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion);

  // Now the device should be registered.
  EXPECT_TRUE(recovery_factor()->IsRegistered());
  trusted_vault_pb::LocalDeviceRegistrationInfo* registration_info =
      GetDeviceRegistrationInfo(account_info());
  EXPECT_TRUE(registration_info->device_registered());
  EXPECT_TRUE(registration_info->has_private_key_material());

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(
          base::as_byte_span(registration_info->private_key_material()));
  EXPECT_THAT(key_pair->public_key().ExportToBytes(),
              Eq(serialized_public_device_key));
}

TEST_F(PhysicalDeviceRecoveryFactorTest, ShouldNotRegisterIfAlreadyRegistered) {
  std::vector<uint8_t> private_device_key = StoreKeysAndMimicDeviceRegistration(
      account_info(), {kVaultKey}, kLastKeyVersion);
  EXPECT_THAT(
      GetDeviceRegistrationInfo(account_info())->device_registered_version(),
      Eq(1));

  // No registration attempt should be made, since device is already registered.
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);

  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(base::DoNothing());
  EXPECT_EQ(
      status,
      TrustedVaultRecoveryFactorRegistrationStateForUMA::kAlreadyRegisteredV1);
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldNotRegisterAfterResetIfAlreadyRegistered) {
  std::vector<uint8_t> private_device_key = StoreKeysAndMimicDeviceRegistration(
      account_info(), {kVaultKey}, kLastKeyVersion);
  ASSERT_THAT(
      GetDeviceRegistrationInfo(account_info())->device_registered_version(),
      Eq(1));

  // Simulate restart.
  ResetRecoveryFactor(account_info());

  // No registration attempt should be made.
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);

  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(base::DoNothing());
  EXPECT_EQ(
      status,
      TrustedVaultRecoveryFactorRegistrationStateForUMA::kAlreadyRegisteredV1);
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldHandleLocalDataObsoleteAndPersistState) {
  StoreKeys(account_info(), {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info()),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _,
          Eq(AuthenticationFactorTypeAndRegistrationParams(
              LocalPhysicalDevice())),
          _))
      .WillOnce([&](const CoreAccountInfo&,
                    const MemberKeysSource& member_keys_source,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Register the device.
  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_EQ(status, TrustedVaultRecoveryFactorRegistrationStateForUMA::
                        kAttemptingRegistrationWithNewKeyPair);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the registration failed with kLocalDataObsolete.
  EXPECT_CALL(register_callback,
              Run(TrustedVaultRegistrationStatus::kLocalDataObsolete, _, _));
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kLocalDataObsolete,
           /*key_version=*/0);

  // Verify persisted file state.
  trusted_vault_pb::LocalTrustedVault proto =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(proto.user_size(), Eq(1));
  // Ensure that the failure is remembered, so there are no retries. This is a
  // regression test for crbug.com/1358015.
  EXPECT_TRUE(proto.user(0).last_registration_returned_local_data_obsolete());
  // Additionally ensure that |local_device_registration_info| has correct
  // state.
  EXPECT_FALSE(
      proto.user(0).local_device_registration_info().device_registered());
  EXPECT_TRUE(proto.user(0)
                  .local_device_registration_info()
                  .has_private_key_material());
  // Keys shouldn't be marked as stale: this is exclusively about upper layers
  // invoking MarkLocalKeysAsStale().
  EXPECT_FALSE(proto.user(0).keys_marked_as_stale_by_consumer());
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldTolerateLocalDataObsoleteChange) {
  StoreKeys(account_info(), {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info()),
          MatchTrustedVaultKeyAndVersions(
              GetTrustedVaultKeysWithVersions({kVaultKey}, kLastKeyVersion)),
          _,
          Eq(AuthenticationFactorTypeAndRegistrationParams(
              LocalPhysicalDevice())),
          _))
      .WillOnce([&](const CoreAccountInfo&,
                    const MemberKeysSource& member_keys_source,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Register the device.
  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_EQ(status, TrustedVaultRecoveryFactorRegistrationStateForUMA::
                        kAttemptingRegistrationWithNewKeyPair);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the local data obsolete flag changed while the network request
  // was in flight.
  storage()
      ->FindUserVault(account_info().gaia)
      ->set_last_registration_returned_local_data_obsolete(true);

  // Pretend that the registration succeeded.
  EXPECT_CALL(register_callback,
              Run(TrustedVaultRegistrationStatus::kSuccess, _, _));
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess,
           /*key_version=*/kLastKeyVersion);

  // Verify persisted file state.
  trusted_vault_pb::LocalTrustedVault proto =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(proto.user_size(), Eq(1));
  // Ensure that the failure is cleared, since registration succeeded.
  EXPECT_FALSE(
      proto.user(0).has_last_registration_returned_local_data_obsolete());
  // Additionally ensure that |local_device_registration_info| has correct
  // state.
  EXPECT_TRUE(
      proto.user(0).local_device_registration_info().device_registered());
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       MarkAsNotRegisteredShouldClearRegistrationData) {
  // Mimic device previously registered with some keys.
  StoreKeysAndMimicDeviceRegistration(account_info(), {kVaultKey},
                                      kLastKeyVersion);
  EXPECT_TRUE(recovery_factor()->IsRegistered());

  recovery_factor()->MarkAsNotRegistered();

  // Now the device should no longer be registered.
  EXPECT_FALSE(recovery_factor()->IsRegistered());
  trusted_vault_pb::LocalDeviceRegistrationInfo* registration_info =
      GetDeviceRegistrationInfo(account_info());
  EXPECT_FALSE(registration_info->device_registered());
  EXPECT_FALSE(registration_info->has_device_registered_version());
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldNotTryToRegisterDeviceIfPreviousAttemptFailed) {
  StoreKeys(account_info(), {kVaultKey}, kLastKeyVersion);
  storage()
      ->FindUserVault(account_info().gaia)
      ->set_last_registration_returned_local_data_obsolete(true);

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);

  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(base::DoNothing());

  EXPECT_EQ(
      status,
      TrustedVaultRecoveryFactorRegistrationStateForUMA::kLocalKeysAreStale);
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldNotAttemptDeviceRegistrationWhenThrottled) {
  EXPECT_CALL(*connection(), AreRequestsThrottled).WillOnce(Return(true));
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterLocalDeviceWithoutKeys).Times(0);

  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(base::DoNothing());

  EXPECT_EQ(
      status,
      TrustedVaultRecoveryFactorRegistrationStateForUMA::kThrottledClientSide);
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldNotAttemptKeyRecoveryWhenNotRegistered) {
  base::test::SingleThreadTaskEnvironment environment;

  EXPECT_CALL(*connection(), DownloadNewKeys).Times(0);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback, Run);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      recovery_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kPhysicalDevice) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered,
      /*expected_bucket_count=*/1);
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldNotAttemptKeyRecoveryWhenThrottled) {
  base::test::SingleThreadTaskEnvironment environment;

  // Mimic device previously registered with some keys.
  StoreKeysAndMimicDeviceRegistration(account_info(), {kVaultKey},
                                      kLastKeyVersion);

  EXPECT_CALL(*connection(), AreRequestsThrottled).WillOnce(Return(true));
  EXPECT_CALL(*connection(), DownloadNewKeys).Times(0);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback, Run);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      recovery_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kPhysicalDevice) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide,
      /*expected_bucket_count=*/1);
}

TEST_F(PhysicalDeviceRecoveryFactorTest, ShouldThrottleKeysDownloading) {
  StoreKeysAndMimicDeviceRegistration(account_info(), {kVaultKey},
                                      kLastKeyVersion);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys(account_info(), _, _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  base::HistogramTester histogram_tester;

  recovery_factor()->AttemptRecovery(recovery_callback.Get());

  ASSERT_FALSE(download_keys_callback.is_null());

  // Mimic failed key downloading, it should record a failed request for
  // throttling.
  EXPECT_CALL(*connection(), RecordFailedRequestForThrottling);
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kOtherError,
           std::vector<std::vector<uint8_t>>(), 0);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kPhysicalDevice) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kOtherError,
      /*expected_bucket_count=*/1);
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldThrottleIfDownloadingReturnedNoNewKeys) {
  StoreKeysAndMimicDeviceRegistration(account_info(), {kVaultKey},
                                      kLastKeyVersion);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys(account_info(), _, _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  base::HistogramTester histogram_tester;

  recovery_factor()->AttemptRecovery(base::DoNothing());

  ASSERT_FALSE(download_keys_callback.is_null());

  // Mimic the server having no new keys.
  EXPECT_CALL(*connection(), RecordFailedRequestForThrottling);
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kNoNewKeys,
           std::vector<std::vector<uint8_t>>(), 0);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kPhysicalDevice) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kNoNewKeys,
      /*expected_bucket_count=*/1);

  Mock::VerifyAndClearExpectations(connection());
}

TEST_F(PhysicalDeviceRecoveryFactorTest, ShouldDownloadNewKeys) {
  const std::vector<std::vector<uint8_t>> kInitialVaultKeys = {{1, 2, 3}};
  const int kInitialLastKeyVersion = 1;

  // Mimic device previously registered with some key.
  StoreKeysAndMimicDeviceRegistration(account_info(), kInitialVaultKeys,
                                      kInitialLastKeyVersion);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(),
          DownloadNewKeys(account_info(),
                          TrustedVaultKeyAndVersionEq(kInitialVaultKeys.back(),
                                                      kInitialLastKeyVersion),
                          _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  base::HistogramTester histogram_tester;

  recovery_factor()->AttemptRecovery(recovery_callback.Get());

  ASSERT_FALSE(download_keys_callback.is_null());

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed.
  const std::vector<std::vector<uint8_t>> kNewVaultKeys = {{1, 2, 3},
                                                           {4, 5, 6}};
  const int kServerLastKeyVersion = kInitialLastKeyVersion + 1;

  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kSuccess, kNewVaultKeys,
                  kServerLastKeyVersion));
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess, kNewVaultKeys,
           kServerLastKeyVersion);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kPhysicalDevice) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/TrustedVaultDownloadKeysStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(PhysicalDeviceRecoveryFactorTest,
       ShouldRegisterWithConstantKeyAndDownloadNewKeys) {
  const int kInitialLastKeyVersion = 1;
  // Perform pre-enrollment.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(*connection(),
              RegisterLocalDeviceWithoutKeys(Eq(account_info()), _, _))
      .WillOnce([&](const CoreAccountInfo& account_info,
                    const SecureBoxPublicKey& device_public_key,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Attempt device registration.
  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  TrustedVaultRecoveryFactorRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_EQ(status, TrustedVaultRecoveryFactorRegistrationStateForUMA::
                        kAttemptingRegistrationWithNewKeyPair);

  // Mimic successful device registration and verify the state.
  EXPECT_CALL(register_callback,
              Run(TrustedVaultRegistrationStatus::kSuccess, _, _));
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kInitialLastKeyVersion);

  // The logic using LocalRecoveryFactor is expected to store the pre-enrollment
  // key. Mimic that behavior.
  StoreKeys(account_info(), {GetConstantTrustedVaultKey()},
            kInitialLastKeyVersion);

  // Attempt recovery of new keys.
  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys(account_info(),
                                         TrustedVaultKeyAndVersionEq(
                                             GetConstantTrustedVaultKey(),
                                             kInitialLastKeyVersion),
                                         _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  recovery_factor()->AttemptRecovery(recovery_callback.Get());

  ASSERT_FALSE(download_keys_callback.is_null());

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed.
  const std::vector<std::vector<uint8_t>> kNewVaultKeys = {
      GetConstantTrustedVaultKey(), {4, 5, 6}};
  const int kServerLastKeyVersion = kInitialLastKeyVersion + 1;

  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kSuccess, kNewVaultKeys,
                  kServerLastKeyVersion));
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess, kNewVaultKeys,
           kServerLastKeyVersion);
}

}  // namespace

}  // namespace trusted_vault
