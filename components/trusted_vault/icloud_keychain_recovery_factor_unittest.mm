// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/icloud_keychain_recovery_factor.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/test/fake_file_access.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SizeIs;

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

    recovery_factor_ = std::make_unique<ICloudKeychainRecoveryFactor>(
        kKeychainAccessGroupPrefix, SecurityDomainId::kChromeSync,
        storage_.get(), connection_.get(), account_info);
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

  std::vector<std::unique_ptr<ICloudRecoveryKey>> RetrieveICloudKeys(
      const trusted_vault::SecurityDomainId security_domain_id) {
    std::vector<std::unique_ptr<ICloudRecoveryKey>> keys;
    base::RunLoop run_loop;
    ICloudRecoveryKey::Retrieve(
        base::BindLambdaForTesting(
            [&](std::vector<std::unique_ptr<ICloudRecoveryKey>> ret) {
              keys = std::move(ret);
              run_loop.Quit();
            }),
        security_domain_id, kKeychainAccessGroup);
    run_loop.Run();
    return keys;
  }

  void AttemptRecoveryAndExpectDownloadRegistrationState(
      DownloadAuthenticationFactorsRegistrationStateResult&&
          download_registration_state_result,
      LocalRecoveryFactor::AttemptRecoveryCallback recovery_callback) {
    base::RunLoop run_loop;
    TrustedVaultConnection::
        DownloadAuthenticationFactorsRegistrationStateCallback
            download_state_callback;
    {
      // A dedicated, nested run loop is required for fetching keys from the
      // iCloud Keychain.
      base::RunLoop fetch_icloud_key_run_loop;
      EXPECT_CALL(*connection(), DownloadAuthenticationFactorsRegistrationState(
                                     account_info(), _, _, _))
          .WillOnce(
              [&](const CoreAccountInfo& account_info,
                  std::set<trusted_vault_pb::SecurityDomainMember_MemberType>
                      recovery_factor_filter,
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
          std::move(recovery_callback).Then(run_loop.QuitClosure()));
      fetch_icloud_key_run_loop.Run();

      CHECK(!download_state_callback.is_null());
    }

    std::move(download_state_callback)
        .Run(std::move(download_registration_state_result));
    run_loop.Run();
  }

  TrustedVaultConnection::DownloadAuthenticationFactorsRegistrationStateCallback
  MaybeRegisterAndExpectDownloadRegistrationState(
      LocalRecoveryFactor::RegisterCallback registration_callback) {
    TrustedVaultConnection::
        DownloadAuthenticationFactorsRegistrationStateCallback
            download_state_callback;

    // A dedicated run loop is required for fetching keys from the iCloud
    // Keychain.
    base::RunLoop fetch_icloud_key_run_loop;
    EXPECT_CALL(*connection(), DownloadAuthenticationFactorsRegistrationState(
                                   account_info(), _, _, _))
        .WillOnce(
            [&](const CoreAccountInfo& account_info,
                std::set<trusted_vault_pb::SecurityDomainMember_MemberType>
                    recovery_factor_filter,
                TrustedVaultConnection::
                    DownloadAuthenticationFactorsRegistrationStateCallback
                        callback,
                base::RepeatingClosure keep_alive_callback) {
              download_state_callback = std::move(callback);
              // Note: Quitting the iCloud Keychain run loop here is ok-ish,
              // because DownloadAuthenticationFactorsRegistrationState is
              // expected to be called after fetching iCloud Keychain keys.
              fetch_icloud_key_run_loop.Quit();
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    TrustedVaultDeviceRegistrationStateForUMA status =
        recovery_factor()->MaybeRegister(std::move(registration_callback));
    CHECK(status == TrustedVaultDeviceRegistrationStateForUMA::
                        kAttemptingRegistrationWithNewKeyPair);
    fetch_icloud_key_run_loop.Run();

    CHECK(!download_state_callback.is_null());
    return download_state_callback;
  }

  void MaybeRegisterAndExpectDownloadRegistrationState(
      DownloadAuthenticationFactorsRegistrationStateResult&&
          download_registration_state_result,
      LocalRecoveryFactor::RegisterCallback registration_callback) {
    base::RunLoop run_loop;
    TrustedVaultConnection::
        DownloadAuthenticationFactorsRegistrationStateCallback
            download_state_callback =
                MaybeRegisterAndExpectDownloadRegistrationState(
                    std::move(registration_callback)
                        .Then(run_loop.QuitClosure()));
    std::move(download_state_callback)
        .Run(std::move(download_registration_state_result));
    run_loop.Run();
  }

  std::unique_ptr<SecureBoxPublicKey>
  MaybeRegisterAndExpectDownloadRegistrationStateAndRegisterAuthenticationFactor(
      std::vector<VaultMember>&& vault_members,
      TrustedVaultRegistrationStatus registration_status,
      int registration_key_version,
      LocalRecoveryFactor::RegisterCallback registration_callback) {
    base::RunLoop run_loop;
    TrustedVaultConnection::
        DownloadAuthenticationFactorsRegistrationStateCallback
            download_state_callback =
                MaybeRegisterAndExpectDownloadRegistrationState(
                    std::move(registration_callback)
                        .Then(run_loop.QuitClosure()));

    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        register_authentication_factor_callback;
    std::unique_ptr<SecureBoxPublicKey> registered_public_key;

    {
      // A dedicated run loop is required for creating the key in the iCloud
      // Keychain.
      base::RunLoop create_icloud_key_run_loop;
      EXPECT_CALL(
          *connection(),
          RegisterAuthenticationFactor(
              Eq(account_info()),
              MatchTrustedVaultKeyAndVersions(
                  GetTrustedVaultKeysWithVersions(kVaultKeys, kLastKeyVersion)),
              _,
              Eq(AuthenticationFactorTypeAndRegistrationParams(
                  ICloudKeychain())),
              _))
          .WillOnce(
              [&](const CoreAccountInfo&,
                  const MemberKeysSource& member_keys_source,
                  const SecureBoxPublicKey& public_key,
                  AuthenticationFactorTypeAndRegistrationParams,
                  TrustedVaultConnection::RegisterAuthenticationFactorCallback
                      callback) {
                register_authentication_factor_callback = std::move(callback);
                registered_public_key = SecureBoxPublicKey::CreateByImport(
                    public_key.ExportToBytes());
                // Note: Quitting the iCloud Keychain run loop here is ok-ish,
                // because RegisterAuthenticationFactor is called directly after
                // creating the iCloud Keychain key.
                create_icloud_key_run_loop.Quit();
                return std::make_unique<TrustedVaultConnection::Request>();
              });

      std::move(download_state_callback)
          .Run(CreateDownloadAuthenticationFactorsRegistrationStateResult(
              DownloadAuthenticationFactorsRegistrationStateResult::State::
                  kRecoverable,
              std::move(vault_members)));

      create_icloud_key_run_loop.Run();

      CHECK(!register_authentication_factor_callback.is_null());
    }

    std::move(register_authentication_factor_callback)
        .Run(registration_status, registration_key_version);
    run_loop.Run();

    return registered_public_key;
  }

  std::unique_ptr<SecureBoxPublicKey>
  MaybeRegisterAndExpectRegisterAuthenticationFactor(
      std::vector<VaultMember>&& vault_members,
      TrustedVaultRegistrationStatus registration_status,
      int registration_key_version,
      LocalRecoveryFactor::RegisterCallback registration_callback) {
    base::RunLoop run_loop;
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        register_authentication_factor_callback;
    std::unique_ptr<SecureBoxPublicKey> registered_public_key;

    {
      // A dedicated run loop is required for fetching keys from the iCloud
      // Keychain and creating a new key.
      base::RunLoop fetch_and_create_icloud_key_run_loop;
      EXPECT_CALL(
          *connection(),
          RegisterAuthenticationFactor(
              Eq(account_info()),
              MatchTrustedVaultKeyAndVersions(
                  GetTrustedVaultKeysWithVersions(kVaultKeys, kLastKeyVersion)),
              _,
              Eq(AuthenticationFactorTypeAndRegistrationParams(
                  ICloudKeychain())),
              _))
          .WillOnce(
              [&](const CoreAccountInfo&,
                  const MemberKeysSource& member_keys_source,
                  const SecureBoxPublicKey& public_key,
                  AuthenticationFactorTypeAndRegistrationParams,
                  TrustedVaultConnection::RegisterAuthenticationFactorCallback
                      callback) {
                register_authentication_factor_callback = std::move(callback);
                registered_public_key = SecureBoxPublicKey::CreateByImport(
                    public_key.ExportToBytes());
                // Note: Quitting the iCloud Keychain run loop here is ok-ish,
                // because RegisterAuthenticationFactor is called directly after
                // creating the iCloud Keychain key.
                fetch_and_create_icloud_key_run_loop.Quit();
                return std::make_unique<TrustedVaultConnection::Request>();
              });

      TrustedVaultDeviceRegistrationStateForUMA status =
          recovery_factor()->MaybeRegister(
              std::move(registration_callback).Then(run_loop.QuitClosure()));
      CHECK_EQ(status, TrustedVaultDeviceRegistrationStateForUMA::
                           kAttemptingRegistrationWithNewKeyPair);
      fetch_and_create_icloud_key_run_loop.Run();

      CHECK(!register_authentication_factor_callback.is_null());
    }

    std::move(register_authentication_factor_callback)
        .Run(registration_status, registration_key_version);
    run_loop.Run();

    return registered_public_key;
  }

  trusted_vault_pb::ICloudKeychainRegistrationInfo* GetICloudRegistrationInfo(
      CoreAccountInfo account_info) {
    trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
        storage_->FindUserVault(account_info.gaia);
    CHECK(per_user_vault);
    return per_user_vault->mutable_icloud_keychain_registration_info();
  }

 private:
  std::unique_ptr<StandaloneTrustedVaultStorage> storage_ = nullptr;
  raw_ptr<FakeFileAccess> file_access_ = nullptr;
  std::unique_ptr<NiceMock<MockTrustedVaultThrottlingConnection>> connection_ =
      nullptr;
  std::unique_ptr<ICloudKeychainRecoveryFactor> recovery_factor_;

  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{kKeychainAccessGroup};
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ICloudKeychainRecoveryFactorTest,
       ShouldNotAttemptKeyRecoveryWithNonConstantKeys) {
  StoreKeys(account_info(), kVaultKeys, kLastKeyVersion);
  EXPECT_CALL(*connection(),
              DownloadAuthenticationFactorsRegistrationState(_, _, _, _))
      .Times(0);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      recovery_callback.Get().Then(run_loop.QuitClosure()));
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
  EXPECT_CALL(*connection(),
              DownloadAuthenticationFactorsRegistrationState(_, _, _, _))
      .Times(0);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      recovery_callback.Get().Then(run_loop.QuitClosure()));
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
  EXPECT_CALL(*connection(),
              DownloadAuthenticationFactorsRegistrationState(_, _, _, _))
      .Times(0);

  CreateICloudKey(SecurityDomainId::kChromeSync);

  base::MockCallback<LocalRecoveryFactor::AttemptRecoveryCallback>
      recovery_callback;
  EXPECT_CALL(recovery_callback,
              Run(LocalRecoveryFactor::RecoveryStatus::kFailure, _, _));
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  recovery_factor()->AttemptRecovery(
      recovery_callback.Get().Then(run_loop.QuitClosure()));
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

TEST_F(ICloudKeychainRecoveryFactorTest,
       AttemptRecoveryShouldFailWithNetworkError) {
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

TEST_F(ICloudKeychainRecoveryFactorTest,
       ShouldNotRegisterWhenAlreadyRegistered) {
  GetICloudRegistrationInfo(account_info())->set_registered(true);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  EXPECT_CALL(register_callback, Run).Times(0);

  TrustedVaultDeviceRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_THAT(
      status,
      Eq(TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1));
}

TEST_F(ICloudKeychainRecoveryFactorTest,
       ShouldNotRegisterWhenLocalDataObsolete) {
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      storage()->FindUserVault(account_info().gaia);
  ASSERT_THAT(per_user_vault, NotNull());
  per_user_vault->set_last_registration_returned_local_data_obsolete(true);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  EXPECT_CALL(register_callback, Run).Times(0);

  TrustedVaultDeviceRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_THAT(
      status,
      Eq(TrustedVaultDeviceRegistrationStateForUMA::kLocalKeysAreStale));
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldNotRegisterWhenThrottled) {
  EXPECT_CALL(*connection(), AreRequestsThrottled).WillOnce(Return(true));

  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  EXPECT_CALL(register_callback, Run).Times(0);

  TrustedVaultDeviceRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_THAT(
      status,
      Eq(TrustedVaultDeviceRegistrationStateForUMA::kThrottledClientSide));
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldNotRegisterWithoutKeys) {
  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  EXPECT_CALL(register_callback, Run).Times(0);

  TrustedVaultDeviceRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_THAT(status, Eq(TrustedVaultDeviceRegistrationStateForUMA::
                             kRegistrationWithConstantKeyNotSupported));
}

TEST_F(ICloudKeychainRecoveryFactorTest, ShouldNotRegisterWithConstantKeys) {
  StoreKeys(account_info(), {GetConstantTrustedVaultKey()}, kLastKeyVersion);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback> register_callback;
  EXPECT_CALL(register_callback, Run).Times(0);

  TrustedVaultDeviceRegistrationStateForUMA status =
      recovery_factor()->MaybeRegister(register_callback.Get());
  EXPECT_THAT(status, Eq(TrustedVaultDeviceRegistrationStateForUMA::
                             kRegistrationWithConstantKeyNotSupported));
}

TEST_F(
    ICloudKeychainRecoveryFactorTest,
    RegistrationShouldFailWithNetworkErrorWhenDownloadRegistrationStateFails) {
  StoreKeys(account_info(), kVaultKeys, kLastKeyVersion);
  CreateICloudKey(SecurityDomainId::kChromeSync);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback>
      registration_callback;
  EXPECT_CALL(*connection(), RecordFailedRequestForThrottling);
  EXPECT_CALL(registration_callback,
              Run(TrustedVaultRegistrationStatus::kNetworkError, _, _));

  // Mimic failed key downloading, it should record a failed request for
  // throttling.
  MaybeRegisterAndExpectDownloadRegistrationState(
      CreateDownloadAuthenticationFactorsRegistrationStateResult(
          DownloadAuthenticationFactorsRegistrationStateResult::State::kError,
          std::vector<VaultMember>()),
      registration_callback.Get());
}

TEST_F(ICloudKeychainRecoveryFactorTest,
       RegistrationShouldDetectAlreadyRegisteredKey) {
  StoreKeys(account_info(), kVaultKeys, kLastKeyVersion);
  std::unique_ptr<ICloudRecoveryKey> icloud_key =
      CreateICloudKey(SecurityDomainId::kChromeSync);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback>
      registration_callback;
  EXPECT_CALL(registration_callback,
              Run(TrustedVaultRegistrationStatus::kAlreadyRegistered,
                  kLastKeyVersion, _));

  std::vector<VaultMember> vault_members;
  vault_members.emplace_back(CreateVaultMember(icloud_key->key()->public_key(),
                                               kVaultKeys, kLastKeyVersion));
  MaybeRegisterAndExpectDownloadRegistrationState(
      CreateDownloadAuthenticationFactorsRegistrationStateResult(
          DownloadAuthenticationFactorsRegistrationStateResult::State::
              kRecoverable,
          std::move(vault_members)),
      registration_callback.Get());

  EXPECT_TRUE(recovery_factor()->IsRegistered());
}

TEST_F(ICloudKeychainRecoveryFactorTest,
       RegistrationShouldHandleLocalDataObsolete) {
  StoreKeys(account_info(), kVaultKeys, kLastKeyVersion);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback>
      registration_callback;

  EXPECT_CALL(registration_callback,
              Run(TrustedVaultRegistrationStatus::kLocalDataObsolete, _, _));
  MaybeRegisterAndExpectRegisterAuthenticationFactor(
      std::vector<VaultMember>(),
      TrustedVaultRegistrationStatus::kLocalDataObsolete,
      /*registration_key_version=*/0, registration_callback.Get());

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      storage()->FindUserVault(account_info().gaia);
  ASSERT_THAT(per_user_vault, NotNull());
  EXPECT_TRUE(per_user_vault->last_registration_returned_local_data_obsolete());
}

TEST_F(ICloudKeychainRecoveryFactorTest, RegistrationShouldSucceed) {
  StoreKeys(account_info(), kVaultKeys, kLastKeyVersion);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback>
      registration_callback;
  EXPECT_CALL(
      registration_callback,
      Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion, _));

  std::unique_ptr<SecureBoxPublicKey> registered_public_key =
      MaybeRegisterAndExpectRegisterAuthenticationFactor(
          std::vector<VaultMember>(), TrustedVaultRegistrationStatus::kSuccess,
          kLastKeyVersion, registration_callback.Get());

  EXPECT_TRUE(recovery_factor()->IsRegistered());
  std::vector<std::unique_ptr<ICloudRecoveryKey>> icloud_keys =
      RetrieveICloudKeys(SecurityDomainId::kChromeSync);
  ASSERT_THAT(icloud_keys, SizeIs(1));
  EXPECT_EQ(icloud_keys[0]->key()->public_key().ExportToBytes(),
            registered_public_key->ExportToBytes());
}

TEST_F(ICloudKeychainRecoveryFactorTest,
       RegistrationShouldSucceedWithUnrelatedKeys) {
  // Create some unrelated iCloud Keychain keys.
  CreateICloudKey(SecurityDomainId::kChromeSync);
  CreateICloudKey(SecurityDomainId::kPasskeys);
  StoreKeys(account_info(), kVaultKeys, kLastKeyVersion);

  base::MockCallback<LocalRecoveryFactor::RegisterCallback>
      registration_callback;

  EXPECT_CALL(
      registration_callback,
      Run(TrustedVaultRegistrationStatus::kSuccess, kLastKeyVersion, _));

  std::vector<VaultMember> vault_members;
  // Return an unrelated iCloud Keychain member.
  vault_members.emplace_back(
      CreateVaultMember(SecureBoxKeyPair::GenerateRandom()->public_key(),
                        kVaultKeys, kLastKeyVersion));
  MaybeRegisterAndExpectDownloadRegistrationStateAndRegisterAuthenticationFactor(
      std::move(vault_members), TrustedVaultRegistrationStatus::kSuccess,
      kLastKeyVersion, registration_callback.Get());

  EXPECT_TRUE(recovery_factor()->IsRegistered());
  std::vector<std::unique_ptr<ICloudRecoveryKey>> icloud_keys =
      RetrieveICloudKeys(SecurityDomainId::kChromeSync);
  // A new key should have been created, in addition to the existing one.
  ASSERT_THAT(icloud_keys, SizeIs(2));
}

TEST_F(ICloudKeychainRecoveryFactorTest,
       MarkAsNotRegisteredShouldClearRegistrationData) {
  GetICloudRegistrationInfo(account_info())->set_registered(true);

  EXPECT_TRUE(recovery_factor()->IsRegistered());

  recovery_factor()->MarkAsNotRegistered();

  // Now the device should no longer be registered.
  EXPECT_FALSE(recovery_factor()->IsRegistered());
  trusted_vault_pb::ICloudKeychainRegistrationInfo* registration_info =
      GetICloudRegistrationInfo(account_info());
  EXPECT_FALSE(registration_info->registered());
}

}  // namespace

}  // namespace trusted_vault
