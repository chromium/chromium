// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/icloud_keychain_recovery_factor.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/fake_file_access.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/fake_apple_keychain_v2.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

constexpr char kKeychainAccessGroupPrefix[] = "accessg_group_prefix";
constexpr char kKeychainAccessGroupSuffix[] = ".com.google.common.folsom";
const std::string kKeychainAccessGroup(
    base::StrCat({kKeychainAccessGroupPrefix, kKeychainAccessGroupSuffix}));

const std::vector<std::vector<uint8_t>> kVaultKeys = {
    GetConstantTrustedVaultKey(),
    {1, 2, 3},
    {4, 5, 6}};
constexpr int kLastKeyVersion = 123;

DownloadAuthenticationFactorsRegistrationStateResult
CreateDownloadAuthenticationFactorsRegistrationStateResult(
    DownloadAuthenticationFactorsRegistrationStateResult::State state,
    std::vector<VaultMember>&& icloud_keys) {
  DownloadAuthenticationFactorsRegistrationStateResult result;
  result.state = state;
  result.icloud_keys = std::move(icloud_keys);
  return result;
}

VaultMember CreateVaultMember(const SecureBoxPublicKey& public_key,
                              const std::vector<MemberKeys>& member_keys) {
  std::vector<MemberKeys> member_keys_copy;
  for (const MemberKeys& member_key : member_keys) {
    member_keys_copy.emplace_back(member_key.version, member_key.wrapped_key,
                                  member_key.proof);
  }
  return VaultMember(
      SecureBoxPublicKey::CreateByImport(public_key.ExportToBytes()),
      std::move(member_keys_copy));
}

VaultMember CreateVaultMember(
    const SecureBoxPublicKey& public_key,
    const std::vector<std::vector<uint8_t>>& vault_keys,
    const int last_key_version) {
  std::vector<MemberKeys> member_keys;
  int cur_version = last_key_version - vault_keys.size();
  for (const auto& vault_key : vault_keys) {
    member_keys.emplace_back(
        /*version=*/++cur_version,
        /*wrapped_key=*/ComputeTrustedVaultWrappedKey(public_key, vault_key),
        /*proof=*/std::vector<uint8_t>());
  }
  return CreateVaultMember(public_key, std::move(member_keys));
}

class ICloudKeychainRecoveryFactorTest : public testing::Test {
 public:
  ICloudKeychainRecoveryFactorTest() { ResetRecoveryFactor(account_info()); }
  ~ICloudKeychainRecoveryFactorTest() override = default;

  void ResetRecoveryFactor(const std::optional<CoreAccountInfo> account_info) {
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
    if (account_info &&
        storage_->FindUserVault(account_info->gaia) == nullptr) {
      storage_->AddUserVault(account_info->gaia);
      storage_->WriteDataToDisk();
    }

    connection_ =
        std::make_unique<NiceMock<MockTrustedVaultThrottlingConnection>>();

    recovery_factor_ = std::make_unique<ICloudKeychainRecoveryFactor>(
        kKeychainAccessGroupPrefix, SecurityDomainId::kChromeSync,
        storage_.get(), account_info);
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

  ICloudKeychainRecoveryFactor* recovery_factor() {
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

  std::unique_ptr<ICloudRecoveryKey> CreateICloudKey(
      const trusted_vault::SecurityDomainId security_domain_id) {
    std::unique_ptr<ICloudRecoveryKey> new_key;
    base::RunLoop run_loop;
    ICloudRecoveryKey::Create(
        base::BindLambdaForTesting([&](std::unique_ptr<ICloudRecoveryKey> ret) {
          new_key = std::move(ret);
          run_loop.Quit();
        }),
        security_domain_id, kKeychainAccessGroup);
    run_loop.Run();
    return new_key;
  }

  void AttemptRecoveryAndExpectDownloadRegistrationState(
      DownloadAuthenticationFactorsRegistrationStateResult&&
          download_registration_state_result,
      LocalRecoveryFactor::AttemptRecoveryCallback recovery_callback) {
    EXPECT_CALL(*connection(), DownloadAuthenticationFactorsRegistrationState);

    base::RunLoop run_loop;
    TrustedVaultConnection::
        DownloadAuthenticationFactorsRegistrationStateCallback
            download_state_callback;
    {
      // A dedicated, nested run loop is required for fetching keys from the
      // iCloud Keychain.
      base::RunLoop fetch_icloud_key_run_loop;
      ON_CALL(*connection(), DownloadAuthenticationFactorsRegistrationState(
                                 account_info(), _, _))
          .WillByDefault(
              [&](const CoreAccountInfo& account_info,
                  TrustedVaultConnection::
                      DownloadAuthenticationFactorsRegistrationStateCallback
                          callback,
                  base::RepeatingClosure keep_alive_callback) {
                download_state_callback = std::move(callback);
                // Note: Quitting the iCloud Keychain run loop here is somewhat
                // hacky, because DownloadAuthenticationFactorsRegistrationState
                // is only called if there were keys in the iCloud Keychain.
                // However, there's no better way to quit the run loop
                // otherwise.
                fetch_icloud_key_run_loop.Quit();
                return std::make_unique<TrustedVaultConnection::Request>();
              });

      recovery_factor()->AttemptRecovery(
          connection(),
          std::move(recovery_callback).Then(run_loop.QuitClosure()));
      fetch_icloud_key_run_loop.Run();

      CHECK(!download_state_callback.is_null());
    }

    std::move(download_state_callback)
        .Run(std::move(download_registration_state_result));
    run_loop.Run();
  }

 private:
  std::unique_ptr<StandaloneTrustedVaultStorage> storage_ = nullptr;
  raw_ptr<FakeFileAccess> file_access_ = nullptr;
  std::unique_ptr<NiceMock<MockTrustedVaultThrottlingConnection>> connection_ =
      nullptr;
  std::unique_ptr<ICloudKeychainRecoveryFactor> recovery_factor_;

  crypto::ScopedFakeAppleKeychainV2 fake_keychain_{kKeychainAccessGroup};
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ICloudKeychainRecoveryFactorTest,
       ShouldNotAttemptKeyRecoveryWithNonConstantKeys) {
  StoreKeys(account_info(), kVaultKeys, kLastKeyVersion);
  EXPECT_CALL(*connection(), DownloadAuthenticationFactorsRegistrationState)
      .Times(0);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      connection(), recovery_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kKeyProofVerificationNotSupported,
      /*expected_bucket_count=*/1);
}

TEST_F(ICloudKeychainRecoveryFactorTest,
       ShouldNotAttemptKeyRecoveryWithNoICloudKeys) {
  EXPECT_CALL(*connection(), DownloadAuthenticationFactorsRegistrationState)
      .Times(0);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      connection(), recovery_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered,
      /*expected_bucket_count=*/1);
}

TEST_F(ICloudKeychainRecoveryFactorTest,
       ShouldNotAttemptKeyRecoveryWhenThrottled) {
  EXPECT_CALL(*connection(), AreRequestsThrottled).WillOnce(Return(true));
  EXPECT_CALL(*connection(), DownloadAuthenticationFactorsRegistrationState)
      .Times(0);

  CreateICloudKey(SecurityDomainId::kChromeSync);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      connection(), recovery_callback.Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide,
      /*expected_bucket_count=*/1);
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldFailWithNetworkError) {
  CreateICloudKey(SecurityDomainId::kChromeSync);

  base::HistogramTester histogram_tester;
  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;

  // Mimic failed key downloading, it should record a failed request for
  // throttling.
  EXPECT_CALL(*connection(), RecordFailedRequestForThrottling);
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  AttemptRecoveryAndExpectDownloadRegistrationState(
      CreateDownloadAuthenticationFactorsRegistrationStateResult(
          DownloadAuthenticationFactorsRegistrationStateResult::State::kError,
          std::vector<VaultMember>()),
      recovery_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kNetworkError,
      /*expected_bucket_count=*/1);
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldFailWithEmptyMembership) {
  std::unique_ptr<ICloudRecoveryKey> icloud_key =
      CreateICloudKey(SecurityDomainId::kChromeSync);

  base::HistogramTester histogram_tester;
  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;

  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  // Return a single member without any member keys.
  std::vector<VaultMember> vault_members;
  vault_members.emplace_back(CreateVaultMember(icloud_key->key()->public_key(),
                                               std::vector<MemberKeys>()));
  AttemptRecoveryAndExpectDownloadRegistrationState(
      CreateDownloadAuthenticationFactorsRegistrationStateResult(
          DownloadAuthenticationFactorsRegistrationStateResult::State::
              kRecoverable,
          std::move(vault_members)),
      recovery_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kMembershipEmpty,
      /*expected_bucket_count=*/1);
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldFailWithCorruptMembership) {
  std::unique_ptr<ICloudRecoveryKey> icloud_key =
      CreateICloudKey(SecurityDomainId::kChromeSync);

  base::HistogramTester histogram_tester;
  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;

  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  // Return a single member with one undecryptable key. Note: proof is not set,
  // because ICloudKeychainRecoveryFactor doesn't use it anyways.
  std::vector<MemberKeys> member_keys;
  member_keys.emplace_back(/*version=*/0,
                           /*wrapped_key=*/std::vector<uint8_t>{7, 8, 9},
                           /*proof=*/std::vector<uint8_t>());
  std::vector<VaultMember> vault_members;
  vault_members.emplace_back(CreateVaultMember(icloud_key->key()->public_key(),
                                               std::move(member_keys)));
  AttemptRecoveryAndExpectDownloadRegistrationState(
      CreateDownloadAuthenticationFactorsRegistrationStateResult(
          DownloadAuthenticationFactorsRegistrationStateResult::State::
              kRecoverable,
          std::move(vault_members)),
      recovery_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kMembershipCorrupted,
      /*expected_bucket_count=*/1);
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldSucceedWithSingleMember) {
  std::unique_ptr<ICloudRecoveryKey> icloud_key =
      CreateICloudKey(SecurityDomainId::kChromeSync);

  base::HistogramTester histogram_tester;
  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;

  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kSuccess, kVaultKeys,
                  kLastKeyVersion));
  // Return a single member with decryptable keys.
  std::vector<VaultMember> vault_members;
  vault_members.emplace_back(CreateVaultMember(icloud_key->key()->public_key(),
                                               kVaultKeys, kLastKeyVersion));
  AttemptRecoveryAndExpectDownloadRegistrationState(
      CreateDownloadAuthenticationFactorsRegistrationStateResult(
          DownloadAuthenticationFactorsRegistrationStateResult::State::
              kRecoverable,
          std::move(vault_members)),
      recovery_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldSucceedWithMultipleMembers) {
  // Create two keys for ChromeSync and one for Passkeys, but only the second
  // one for ChromeSync will be member of the security domain.
  CreateICloudKey(SecurityDomainId::kChromeSync);
  std::unique_ptr<ICloudRecoveryKey> local_icloud_key =
      CreateICloudKey(SecurityDomainId::kChromeSync);
  CreateICloudKey(SecurityDomainId::kPasskeys);

  // Unrelated key which is in the security domain, but not available in the
  // iCloud Keychain.
  std::unique_ptr<SecureBoxKeyPair> other_icloud_key =
      SecureBoxKeyPair::GenerateRandom();

  base::HistogramTester histogram_tester;
  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;

  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kSuccess, kVaultKeys,
                  kLastKeyVersion));
  // Return two members, the first one's keys aren't available in the iCloud
  // Keychain.
  std::vector<VaultMember> vault_members;
  vault_members.emplace_back(CreateVaultMember(other_icloud_key->public_key(),
                                               kVaultKeys, kLastKeyVersion));
  vault_members.emplace_back(CreateVaultMember(
      local_icloud_key->key()->public_key(), kVaultKeys, kLastKeyVersion));
  AttemptRecoveryAndExpectDownloadRegistrationState(
      CreateDownloadAuthenticationFactorsRegistrationStateResult(
          DownloadAuthenticationFactorsRegistrationStateResult::State::
              kRecoverable,
          std::move(vault_members)),
      recovery_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.DownloadKeysStatus." +
          GetLocalRecoveryFactorNameForUma(
              LocalRecoveryFactorType::kICloudKeychain) +
          "." + GetSecurityDomainNameForUma(SecurityDomainId::kChromeSync),
      /*sample=*/
      TrustedVaultDownloadKeysStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);
}

}  // namespace

}  // namespace trusted_vault
