// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_sync_test_devices.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_ecies_encryptor.h"
#include "chromeos/ash/services/device_sync/fake_ecies_encryption.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kAccessTokenUsed[] = "access token used by CryptAuthClient";

const cryptauthv2::ClientMetadata& GetClientMetadata() {
  static const base::NoDestructor<cryptauthv2::ClientMetadata> client_metadata(
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::PERIODIC));
  return *client_metadata;
}

const cryptauthv2::RequestContext& GetRequestContext() {
  static const base::NoDestructor<cryptauthv2::RequestContext> request_context(
      [] {
        return cryptauthv2::BuildRequestContext(
            CryptAuthKeyBundle::KeyBundleNameEnumToString(
                CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether),
            GetClientMetadata(),
            cryptauthv2::GetClientAppMetadataForTest().instance_id(),
            cryptauthv2::GetClientAppMetadataForTest().instance_id_token());
      }());
  return *request_context;
}

const CryptAuthKey& GetGroupKey() {
  static const base::NoDestructor<CryptAuthKey> group_key([] {
    return CryptAuthKey(
        kGroupPublicKey, GetPrivateKeyFromPublicKeyForTest(kGroupPublicKey),
        CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);
  }());
  return *group_key;
}

CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap
IdToEncryptingKeyMapFromDeviceIds(
    const base::flat_set<std::string>& device_ids) {
  CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap id_to_encrypting_key_map;
  for (const std::string& id : device_ids) {
    id_to_encrypting_key_map.insert_or_assign(
        id, GetTestDeviceWithId(id).device_better_together_public_key);
  }

  return id_to_encrypting_key_map;
}

}  // namespace

class DeviceSyncCryptAuthGroupPrivateKeySharerImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 public:
  DeviceSyncCryptAuthGroupPrivateKeySharerImplTest(
      const DeviceSyncCryptAuthGroupPrivateKeySharerImplTest&) = delete;
  DeviceSyncCryptAuthGroupPrivateKeySharerImplTest& operator=(
      const DeviceSyncCryptAuthGroupPrivateKeySharerImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthGroupPrivateKeySharerImplTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS)),
        fake_cryptauth_ecies_encryptor_factory_(
            std::make_unique<FakeCryptAuthEciesEncryptorFactory>()) {
    client_factory_->AddObserver(this);
  }

  ~DeviceSyncCryptAuthGroupPrivateKeySharerImplTest() override {
    client_factory_->RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    CryptAuthEciesEncryptorImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_ecies_encryptor_factory_.get());

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = mock_timer.get();

    sharer_ = CryptAuthGroupPrivateKeySharerImpl::Factory::Create(
        client_factory_.get(), std::move(mock_timer));
  }

  // testing::Test:
  void TearDown() override {
    CryptAuthEciesEncryptorImpl::Factory::SetFactoryForTesting(nullptr);
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client, ShareGroupPrivateKey(testing::_, testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &DeviceSyncCryptAuthGroupPrivateKeySharerImplTest::
                             OnShareGroupPrivateKey));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(testing::Return(kAccessTokenUsed));
  }

  void ShareGroupPrivateKey(
      const CryptAuthKey& group_key,
      const CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap&
          id_to_encrypting_key_map) {
    group_key_ = std::make_unique<CryptAuthKey>(group_key);
    id_to_encrypting_key_map_ = id_to_encrypting_key_map;

    sharer_->ShareGroupPrivateKey(
        GetRequestContext(), group_key, id_to_encrypting_key_map,
        base::BindOnce(&DeviceSyncCryptAuthGroupPrivateKeySharerImplTest::
                           OnShareGroupPrivateKeyComplete,
                       base::Unretained(this)));
  }

  // Fail encryption for IDs in |device_ids_to_fail|. Encryption could fail if
  // the input encrypting key is invalid, for instance.
  void RunGroupPrivateKeyEncryptor(
      const base::flat_set<std::string>& expected_device_ids,
      const base::flat_set<std::string>& device_ids_to_fail) {
    ASSERT_EQ(expected_device_ids.size(),
              encryptor()->id_to_input_map().size());

    for (const auto& id_payload_and_key_pair : encryptor()->id_to_input_map()) {
      const std::string& id = id_payload_and_key_pair.first;
      const std::string& payload = id_payload_and_key_pair.second.payload;
      const std::string& encrypting_key = id_payload_and_key_pair.second.key;

      EXPECT_TRUE(base::Contains(expected_device_ids, id));

      // Verify that encryptor inputs agrees with ShareGroupPrivateKey() inputs.
      const auto it = id_to_encrypting_key_map_.find(id);
      ASSERT_NE(id_to_encrypting_key_map_.end(), it);
      EXPECT_EQ(it->second, encrypting_key);
      ASSERT_TRUE(group_key_);
      ASSERT_TRUE(!group_key_->private_key().empty());
      EXPECT_EQ(group_key_->private_key(), payload);

      id_to_encrypted_group_private_key_map_[id] =
          base::Contains(device_ids_to_fail, id)
              ? std::nullopt
              : std::make_optional<std::string>(
                    MakeFakeEncryptedString(payload, encrypting_key));
    }

    encryptor()->FinishAttempt(FakeCryptAuthEciesEncryptor::Action::kEncryption,
                               id_to_encrypted_group_private_key_map_);
  }

  // Ensures that ShareGroupPrivateKeyRequest is consistent with the output from
  // the encryptor, |id_to_encrypted_group_private_key_map_|.
  void VerifyShareGroupPrivateKeyRequest(
      const base::flat_set<std::string>& expected_device_ids) {
    ASSERT_TRUE(share_group_private_key_request_);
    EXPECT_TRUE(share_group_private_key_success_callback_);
    EXPECT_TRUE(share_group_private_key_failure_callback_);

    EXPECT_EQ(GetRequestContext().SerializeAsString(),
              share_group_private_key_request_->context().SerializeAsString());
    EXPECT_EQ(
        static_cast<int>(expected_device_ids.size()),
        share_group_private_key_request_->encrypted_group_private_keys_size());

    for (const cryptauthv2::EncryptedGroupPrivateKey& request_encrypted_key :
         share_group_private_key_request_->encrypted_group_private_keys()) {
      const std::string& recipient_id =
          request_encrypted_key.recipient_device_id();

      const auto expected_it =
          id_to_encrypted_group_private_key_map_.find(recipient_id);
      ASSERT_NE(id_to_encrypted_group_private_key_map_.end(), expected_it);
      ASSERT_TRUE(expected_it->second);

      EXPECT_EQ(GetRequestContext().device_id(),
                request_encrypted_key.sender_device_id());
      EXPECT_EQ(kGroupPublicKeyHash,
                request_encrypted_key.group_public_key_hash());
      EXPECT_EQ(*expected_it->second,
                request_encrypted_key.encrypted_private_key());

      // Verify that the encrypted group private key can be decrypted with the
      // recipient device's private key.
      std::string recipient_device_better_together_private_key =
          GetPrivateKeyFromPublicKeyForTest(
              GetTestDeviceWithId(recipient_id)
                  .device_better_together_public_key);
      EXPECT_EQ(group_key_->private_key(),
                DecryptFakeEncryptedString(
                    request_encrypted_key.encrypted_private_key(),
                    recipient_device_better_together_private_key));
    }
  }

  void SendShareGroupPrivateKeyResponse() {
    ASSERT_TRUE(share_group_private_key_success_callback_);
    std::move(share_group_private_key_success_callback_)
        .Run(cryptauthv2::ShareGroupPrivateKeyResponse());
  }

  void FailShareGroupPrivateKeyRequest(
      const NetworkRequestError& network_request_error) {
    ASSERT_TRUE(share_group_private_key_failure_callback_);
    std::move(share_group_private_key_failure_callback_)
        .Run(network_request_error);
  }

  void VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode expected_result_code) {
    ASSERT_TRUE(device_sync_result_code_);
    EXPECT_EQ(expected_result_code, device_sync_result_code_);
  }

  base::MockOneShotTimer* timer() { return timer_; }

 private:
  FakeCryptAuthEciesEncryptor* encryptor() {
    return fake_cryptauth_ecies_encryptor_factory_->instance();
  }

  void OnShareGroupPrivateKey(
      const cryptauthv2::ShareGroupPrivateKeyRequest& request,
      CryptAuthClient::ShareGroupPrivateKeyCallback callback,
      CryptAuthClient::ErrorCallback error_callback) {
    EXPECT_FALSE(share_group_private_key_request_);
    EXPECT_FALSE(share_group_private_key_success_callback_);
    EXPECT_FALSE(share_group_private_key_failure_callback_);

    share_group_private_key_request_ = request;
    share_group_private_key_success_callback_ = std::move(callback);
    share_group_private_key_failure_callback_ = std::move(error_callback);
  }

  void OnShareGroupPrivateKeyComplete(
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
    device_sync_result_code_ = device_sync_result_code;
  }

  std::unique_ptr<CryptAuthKey> group_key_;
  CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap
      id_to_encrypting_key_map_;

  std::optional<cryptauthv2::ShareGroupPrivateKeyRequest>
      share_group_private_key_request_;
  CryptAuthClient::ShareGroupPrivateKeyCallback
      share_group_private_key_success_callback_;
  CryptAuthClient::ErrorCallback share_group_private_key_failure_callback_;

  CryptAuthEciesEncryptor::IdToOutputMap id_to_encrypted_group_private_key_map_;

  std::optional<CryptAuthDeviceSyncResult::ResultCode> device_sync_result_code_;

  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;
  std::unique_ptr<FakeCryptAuthEciesEncryptorFactory>
      fake_cryptauth_ecies_encryptor_factory_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;

  std::unique_ptr<CryptAuthGroupPrivateKeySharer> sharer_;
};

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest, Success) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();
  ShareGroupPrivateKey(GetGroupKey(),
                       IdToEncryptingKeyMapFromDeviceIds(device_ids));

  RunGroupPrivateKeyEncryptor(device_ids, {} /* device_ids_to_fail */);

  VerifyShareGroupPrivateKeyRequest(device_ids);

  SendShareGroupPrivateKeyResponse();

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest,
       FinishedWithNonFatalErrors_SingleEncryptionFails) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();
  ShareGroupPrivateKey(GetGroupKey(),
                       IdToEncryptingKeyMapFromDeviceIds(device_ids));

  // Encryption fails for a remote device.
  std::string encryption_failure_device_id =
      GetRemoteDeviceNeedsGroupPrivateKeyForTest().instance_id();
  RunGroupPrivateKeyEncryptor(
      device_ids, {encryption_failure_device_id} /* device_ids_to_fail */);

  base::flat_set<std::string> expected_device_ids = device_ids;
  expected_device_ids.erase(encryption_failure_device_id);
  VerifyShareGroupPrivateKeyRequest(expected_device_ids);

  SendShareGroupPrivateKeyResponse();

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest,
       Failure_AllEncryptionsFails) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();
  ShareGroupPrivateKey(GetGroupKey(),
                       IdToEncryptingKeyMapFromDeviceIds(device_ids));

  // Encryption fails for all devices.
  RunGroupPrivateKeyEncryptor(device_ids, device_ids /* device_ids_to_fail */);

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingGroupPrivateKey);
}

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest,
       FinishedWithNonFatalErrors_SingleEncryptionKeyEmpty) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();
  CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap
      id_to_encrypting_key_map = IdToEncryptingKeyMapFromDeviceIds(device_ids);

  // A remote device has an empty encrypting key.
  std::string empty_encrypting_key_device_id =
      GetRemoteDeviceNeedsGroupPrivateKeyForTest().instance_id();
  id_to_encrypting_key_map[empty_encrypting_key_device_id].clear();
  ShareGroupPrivateKey(GetGroupKey(), id_to_encrypting_key_map);

  base::flat_set<std::string> expected_device_ids = device_ids;
  expected_device_ids.erase(empty_encrypting_key_device_id);
  RunGroupPrivateKeyEncryptor(expected_device_ids, {} /* device_ids_to_fail */);

  VerifyShareGroupPrivateKeyRequest(expected_device_ids);

  SendShareGroupPrivateKeyResponse();

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest,
       Failure_AllEncryptionKeysEmpty) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();

  // All devices have an empty encrypting key.
  CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap
      id_to_encrypting_key_map = IdToEncryptingKeyMapFromDeviceIds(device_ids);
  for (auto& id_encrypting_key_pair : id_to_encrypting_key_map)
    id_encrypting_key_pair.second.clear();

  ShareGroupPrivateKey(GetGroupKey(), id_to_encrypting_key_map);

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingGroupPrivateKey);
}

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest,
       Failure_Timeout_Encryption) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();
  ShareGroupPrivateKey(GetGroupKey(),
                       IdToEncryptingKeyMapFromDeviceIds(device_ids));

  timer()->Fire();

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForGroupPrivateKeyEncryption);
}

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest,
       Failure_Timeout_ShareGroupPrivateKeyRequest) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();
  ShareGroupPrivateKey(GetGroupKey(),
                       IdToEncryptingKeyMapFromDeviceIds(device_ids));

  RunGroupPrivateKeyEncryptor(device_ids, {} /* device_ids_to_fail */);

  VerifyShareGroupPrivateKeyRequest(device_ids);

  timer()->Fire();

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForShareGroupPrivateKeyResponse);
}

TEST_F(DeviceSyncCryptAuthGroupPrivateKeySharerImplTest,
       Failure_ApiCall_ShareGroupPrivateKey) {
  base::flat_set<std::string> device_ids = GetAllTestDeviceIds();
  ShareGroupPrivateKey(GetGroupKey(),
                       IdToEncryptingKeyMapFromDeviceIds(device_ids));

  RunGroupPrivateKeyEncryptor(device_ids, {} /* device_ids_to_fail */);

  VerifyShareGroupPrivateKeyRequest(device_ids);

  FailShareGroupPrivateKeyRequest(NetworkRequestError::kBadRequest);

  VerifyShareGroupPrivateKeyResult(
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallBadRequest);
}

}  // namespace device_sync

}  // namespace ash
