// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/recoverable_key_store.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/recovery_key_provider_ash.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/recovery_key_store_controller.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/mock_recovery_key_store_connection.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {
namespace {

using base::test::RunOnceCallback;
using testing::_;
using testing::AllOf;
using testing::IsEmpty;
using testing::Return;
using testing::StrictMock;

constexpr char kTestGaiaId[] = "test_gaia_id";
constexpr char kTestUserEmail[] = "user@example.com";
constexpr char kSecurityDomainKeyName[] =
    "security_domain_member_key_encrypted_locally";
constexpr char kTestDeviceId[] = "test device id";
const user_data_auth::GetRecoverableKeyStoresReply
    kEmptyGetRecoverableKeyStoresReply = {};
constexpr base::TimeDelta kTestUpdatePeriod = base::Minutes(30);

MATCHER_P(HasAccountId, expected, "") {
  const user_data_auth::GetRecoverableKeyStoresRequest& request = arg;
  const AccountId& account_id = expected;
  return request.account_id().account_id() == account_id.GetUserEmail();
}

MATCHER_P(DeviceIdEquals, expected, "") {
  const trusted_vault_pb::Vault& request = arg;
  return request.chrome_os_metadata().device_id() == expected;
}

MATCHER(VaultHasPasskeysApplicationKey, "") {
  const trusted_vault_pb::Vault& request = arg;
  auto application_keys = request.application_keys();
  return application_keys.size() == 1 &&
         application_keys.at(0).key_name() == kSecurityDomainKeyName;
}

MATCHER_P(RecoveryKeyStoreStatePublicKeyEq, expected, "") {
  const trusted_vault_pb::RecoveryKeyStoreState& state = arg;
  std::vector<uint8_t> public_key{state.public_key().begin(),
                                  state.public_key().end()};
  return public_key == expected;
}

static const std::vector<uint8_t> kPublicKeyBytes =
    SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();

class GetRecoverableKeyStoresReplyBuilder {
 public:
  GetRecoverableKeyStoresReplyBuilder() = default;
  ~GetRecoverableKeyStoresReplyBuilder() = default;

  user_data_auth::GetRecoverableKeyStoresReply Build() { return reply_; }

  GetRecoverableKeyStoresReplyBuilder& AddPinKeyStore() {
    cryptohome::RecoverableKeyStore* key_store = reply_.add_key_stores();
    key_store->mutable_key_store_metadata()->set_knowledge_factor_type(
        cryptohome::KNOWLEDGE_FACTOR_TYPE_PIN);
    key_store->mutable_key_store_metadata()->set_hash_type(
        cryptohome::HASH_TYPE_PBKDF2_AES256_1234);
    key_store->mutable_wrapped_security_domain_key()->set_key_name(
        kSecurityDomainKeyName);
    AssignBytesToProtoString(
        kPublicKeyBytes,
        key_store->mutable_wrapped_security_domain_key()->mutable_public_key());
    return *this;
  }

  GetRecoverableKeyStoresReplyBuilder& AddPasswordKeyStore() {
    cryptohome::RecoverableKeyStore* key_store = reply_.add_key_stores();
    key_store->mutable_key_store_metadata()->set_knowledge_factor_type(
        cryptohome::KNOWLEDGE_FACTOR_TYPE_PASSWORD);
    key_store->mutable_key_store_metadata()->set_hash_type(
        cryptohome::HASH_TYPE_SHA256_TOP_HALF);
    key_store->mutable_wrapped_security_domain_key()->set_key_name(
        kSecurityDomainKeyName);
    AssignBytesToProtoString(
        kPublicKeyBytes,
        key_store->mutable_wrapped_security_domain_key()->mutable_public_key());
    return *this;
  }

 private:
  user_data_auth::GetRecoverableKeyStoresReply reply_ = {};
};

class FakeRecoveryKeyStoreControllerDelegate
    : public RecoveryKeyStoreController::Delegate {
 public:
  FakeRecoveryKeyStoreControllerDelegate() = default;
  ~FakeRecoveryKeyStoreControllerDelegate() = default;

  const trusted_vault_pb::RecoveryKeyStoreState& state() { return state_; }

  void WriteRecoveryKeyStoreState(
      const trusted_vault_pb::RecoveryKeyStoreState& state) override {
    state_ = state;
  }

  void AddRecoveryKeyToSecurityDomain(
      const std::vector<uint8_t>& public_key,
      RecoveryKeyRegistrationCallback callback) override {
    std::move(callback).Run(TrustedVaultRegistrationStatus::kSuccess);
  }

 private:
  trusted_vault_pb::RecoveryKeyStoreState state_;
};

CoreAccountInfo TestAccountInfo() {
  CoreAccountInfo account_info;
  account_info.gaia = kTestGaiaId;
  account_info.email = kTestUserEmail;
  account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
  return account_info;
}

class RecoveryKeyStoreControllerAshTest : public testing::Test {
 protected:
  RecoveryKeyStoreControllerAshTest() {
    ash::UserDataAuthClient::OverrideGlobalInstanceForTesting(&user_data_auth_);
    connection_holder_ =
        std::make_unique<testing::StrictMock<MockRecoveryKeyStoreConnection>>();
    connection_ = connection_holder_.get();
  }

  void ExpectGetRecoverableKeyStoresCallAndReply(
      const user_data_auth::GetRecoverableKeyStoresReply& reply) {
    EXPECT_CALL(user_data_auth_,
                GetRecoverableKeyStores(HasAccountId(account_id_), _))
        .WillOnce(RunOnceCallback<1>(reply));
  }

  void ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus status) {
    EXPECT_CALL(*connection_,
                UpdateRecoveryKeyStore(account_info_,
                                       AllOf(DeviceIdEquals(kTestDeviceId),
                                             VaultHasPasskeysApplicationKey()),
                                       _))
        .WillOnce(
            [status](const CoreAccountInfo& account_info,
                     const trusted_vault_pb::Vault& request,
                     RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback
                         callback) {
              // Invoke `callback` asynchronously to avoid hair pinning.
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), status));
              return std::make_unique<RecoveryKeyStoreConnection::Request>();
            });
  }

  void StartController(base::Time last_update = base::Time::UnixEpoch(),
                       base::TimeDelta update_period = base::TimeDelta::Max()) {
    CHECK(!controller_);
    CHECK(connection_holder_);
    auto recovery_key_provider = std::make_unique<RecoveryKeyProviderAsh>(
        base::SequencedTaskRunner::GetCurrentDefault(), account_id_,
        kTestDeviceId);
    controller_ = std::make_unique<RecoveryKeyStoreController>(
        std::move(recovery_key_provider), std::move(connection_holder_),
        &delegate_);
    trusted_vault_pb::RecoveryKeyStoreState last_state;
    last_state.set_recovery_key_store_upload_enabled(true);
    last_state.set_last_recovery_key_store_update_millis_since_unix_epoch(
        last_update.InMillisecondsSinceUnixEpoch());
    controller_->StartPeriodicUploads(account_info_, last_state, update_period);
  }

  void StopController() { controller_->StopPeriodicUploads(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
  CoreAccountInfo account_info_ = TestAccountInfo();

  testing::StrictMock<ash::MockUserDataAuthClient> user_data_auth_;
  std::unique_ptr<RecoveryKeyStoreController> controller_;
  raw_ptr<MockRecoveryKeyStoreConnection> connection_;
  std::unique_ptr<MockRecoveryKeyStoreConnection> connection_holder_;
  FakeRecoveryKeyStoreControllerDelegate delegate_;
};

TEST_F(RecoveryKeyStoreControllerAshTest,
       ShouldFailUpdateRecoveryKeyStoreWithEmptyVaults) {
  ExpectGetRecoverableKeyStoresCallAndReply(kEmptyGetRecoverableKeyStoresReply);
  StartController();
  task_environment_.RunUntilIdle();
}

TEST_F(RecoveryKeyStoreControllerAshTest,
       ShouldSuccessfullyMakeUpdateRecoveryKeyStoreRequestWithPinVault) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  StartController();
  task_environment_.RunUntilIdle();
  EXPECT_THAT(delegate_.state(),
              RecoveryKeyStoreStatePublicKeyEq(kPublicKeyBytes));
}

TEST_F(RecoveryKeyStoreControllerAshTest,
       ShouldSuccessfullyMakeUpdateRecoveryKeyStoreRequestWithPasswordVault) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPasswordKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  StartController();
  task_environment_.RunUntilIdle();
  EXPECT_THAT(delegate_.state(),
              RecoveryKeyStoreStatePublicKeyEq(kPublicKeyBytes));
}

TEST_F(
    RecoveryKeyStoreControllerAshTest,
    ShouldSuccessfullyMakeUpdateRecoveryKeyStoreRequestWithPinAndPasswordVault) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder()
          .AddPasswordKeyStore()
          .AddPinKeyStore()
          .Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  StartController();
  task_environment_.RunUntilIdle();
  EXPECT_THAT(delegate_.state(),
              RecoveryKeyStoreStatePublicKeyEq(kPublicKeyBytes));
}

TEST_F(RecoveryKeyStoreControllerAshTest,
       ShouldHandleUpdateRecoveryKeyStoreConnectionError) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kOtherError);
  StartController();
  task_environment_.RunUntilIdle();
}

TEST_F(RecoveryKeyStoreControllerAshTest,
       ShouldImmediatelyScheduleUpdateIfOverdue) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  StartController(
      /*last_update=*/base::Time::Now() - kTestUpdatePeriod - base::Seconds(1),
      /*update_period=*/kTestUpdatePeriod);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(delegate_.state(),
              RecoveryKeyStoreStatePublicKeyEq(kPublicKeyBytes));
}

TEST_F(RecoveryKeyStoreControllerAshTest, ShouldScheduleUpdateAfterSuccess) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  StartController(/*last_update=*/base::Time(),
                  /*update_period=*/kTestUpdatePeriod);

  // After the initial upload, the next one should occur after
  // `kTestUpdatePeriod` elapsed.
  task_environment_.FastForwardBy(kTestUpdatePeriod - base::Seconds(1));

  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  task_environment_.FastForwardBy(base::Seconds(1) + base::Milliseconds(1));
  EXPECT_THAT(delegate_.state(),
              RecoveryKeyStoreStatePublicKeyEq(kPublicKeyBytes));
}

TEST_F(RecoveryKeyStoreControllerAshTest, ShouldScheduleUpdateAfterError) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kOtherError);
  StartController(/*last_update=*/base::Time(),
                  /*update_period=*/kTestUpdatePeriod);

  // After an upload failed with an error, the next one should occur regularly
  // with the next `kTestUpdatePeriod`.
  // TODO: crbug.com/1223853 - Verify desired behavior.
  task_environment_.FastForwardBy(kTestUpdatePeriod - base::Seconds(1));

  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  task_environment_.FastForwardBy(base::Seconds(1) + base::Milliseconds(1));
  EXPECT_THAT(delegate_.state(),
              RecoveryKeyStoreStatePublicKeyEq(kPublicKeyBytes));
}

TEST_F(RecoveryKeyStoreControllerAshTest, ShouldCeaseUploadsAfterStopping) {
  ExpectGetRecoverableKeyStoresCallAndReply(
      GetRecoverableKeyStoresReplyBuilder().AddPinKeyStore().Build());
  ExpectConnectionUpdateRecoveryKeyStoreCallAndReply(
      UpdateRecoveryKeyStoreStatus::kSuccess);
  StartController(/*last_update=*/base::Time(),
                  /*update_period=*/kTestUpdatePeriod);

  // Allow the initial upload to proceed.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(delegate_.state(),
              RecoveryKeyStoreStatePublicKeyEq(kPublicKeyBytes));

  StopController();

  // No further uploads should occur.
  task_environment_.FastForwardBy(2 * kTestUpdatePeriod);
}

}  // namespace
}  // namespace trusted_vault
