// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/mock_timer.h"
#include "base/types/optional_util.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_sync_test_devices.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_ecies_encryptor.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_key_creator.h"
#include "chromeos/ash/services/device_sync/fake_ecies_encryption.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

// Note: Include a non-UTF-8 character to ensure that we handle them correctly
// in prefs.
const char kStaleGroupPublicKey[] = "stale_group_public_key\xff";

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

}  // namespace

class DeviceSyncCryptAuthMetadataSyncerImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 public:
  DeviceSyncCryptAuthMetadataSyncerImplTest(
      const DeviceSyncCryptAuthMetadataSyncerImplTest&) = delete;
  DeviceSyncCryptAuthMetadataSyncerImplTest& operator=(
      const DeviceSyncCryptAuthMetadataSyncerImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthMetadataSyncerImplTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS)),
        fake_cryptauth_key_creator_factory_(
            std::make_unique<FakeCryptAuthKeyCreatorFactory>()),
        fake_cryptauth_ecies_encryptor_factory_(
            std::make_unique<FakeCryptAuthEciesEncryptorFactory>()) {
    client_factory_->AddObserver(this);
  }

  ~DeviceSyncCryptAuthMetadataSyncerImplTest() override {
    client_factory_->RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    CryptAuthKeyCreatorImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_key_creator_factory_.get());
    CryptAuthEciesEncryptorImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_ecies_encryptor_factory_.get());

    CryptAuthMetadataSyncerImpl::RegisterPrefs(pref_service_.registry());
  }

  // testing::Test:
  void TearDown() override {
    CryptAuthKeyCreatorImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthEciesEncryptorImpl::Factory::SetFactoryForTesting(nullptr);
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client, SyncMetadata(testing::_, testing::_, testing::_))
        .WillByDefault(Invoke(this, &DeviceSyncCryptAuthMetadataSyncerImplTest::
                                        OnSyncMetadataResponse));

    ON_CALL(*client, GetAccessTokenUsed())
        .WillByDefault(testing::Return(kAccessTokenUsed));
  }

  void SetLocalDeviceMetadataPrefs(
      const std::string& unencrypted_local_device_metadata,
      const std::string& group_public_key) {
    pref_service_.SetString(
        prefs::kCryptAuthLastSyncedUnencryptedLocalDeviceMetadata,
        util::EncodeAsString(unencrypted_local_device_metadata));
    pref_service_.SetString(prefs::kCryptAuthLastSyncedGroupPublicKey,
                            util::EncodeAsString(group_public_key));
    pref_service_.SetString(
        prefs::kCryptAuthLastSyncedEncryptedLocalDeviceMetadata,
        util::EncodeAsString(MakeFakeEncryptedString(
            unencrypted_local_device_metadata, group_public_key)));
  }

  void SyncMetadata(
      const std::optional<std::string>& initial_group_public_key,
      const std::optional<std::string>& initial_group_private_key) {
    if (initial_group_public_key) {
      initial_group_key_ = CryptAuthKey(
          *initial_group_public_key,
          initial_group_private_key.value_or(std::string()),
          CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);
    }

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = mock_timer.get();

    metadata_syncer_ = CryptAuthMetadataSyncerImpl::Factory::Create(
        client_factory_.get(), &pref_service_, std::move(mock_timer));

    metadata_syncer_->SyncMetadata(
        GetRequestContext(),
        *GetLocalDeviceForTest().better_together_device_metadata,
        base::OptionalToPtr(initial_group_key_),
        base::BindOnce(
            &DeviceSyncCryptAuthMetadataSyncerImplTest::OnMetadataSyncComplete,
            base::Unretained(this)));
  }

  void RunKeyCreator(bool success,
                     const std::string& group_public_key,
                     const std::string& group_private_key) {
    ASSERT_TRUE(key_creator()->create_keys_callback());
    ASSERT_EQ(1u, key_creator()->keys_to_create().size());
    EXPECT_FALSE(key_creator()->server_ephemeral_dh());

    const auto it = key_creator()->keys_to_create().find(
        CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey);
    ASSERT_NE(key_creator()->keys_to_create().end(), it);
    EXPECT_EQ(CryptAuthKey::Status::kActive, it->second.status);
    EXPECT_EQ(cryptauthv2::KeyType::P256, it->second.type);

    std::move(key_creator()->create_keys_callback())
        .Run({{CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey,
               success ? std::make_optional(
                             CryptAuthKey(group_public_key, group_private_key,
                                          CryptAuthKey::Status::kActive,
                                          cryptauthv2::KeyType::P256))
                       : std::nullopt}},
             std::nullopt /* client_ephemeral_dh_output */);
  }

  void RunLocalBetterTogetherMetadataEncryptor(
      const std::string& expected_group_public_key,
      bool succeed) {
    ASSERT_EQ(1u, encryptor()->id_to_input_map().size());
    const auto it = encryptor()->id_to_input_map().begin();
    EXPECT_EQ(GetLocalDeviceForTest()
                  .better_together_device_metadata->SerializeAsString(),
              it->second.payload);
    EXPECT_EQ(expected_group_public_key, it->second.key);

    std::optional<std::string> encrypted_metadata =
        succeed ? std::optional<std::string>(MakeFakeEncryptedString(
                      it->second.payload, it->second.key))
                : std::nullopt;
    encryptor()->FinishAttempt(FakeCryptAuthEciesEncryptor::Action::kEncryption,
                               {{it->first, encrypted_metadata}});
  }

  void VerifyFirstSyncMetadataRequest(
      const std::string& expected_group_public_key,
      bool expected_need_group_private_key) {
    VerifySyncMetadataRequest(expected_group_public_key,
                              expected_need_group_private_key,
                              true /* is_first */);
  }

  void SendFirstSyncMetadataResponse(
      const std::vector<cryptauthv2::DeviceMetadataPacket>&
          device_metadata_packets,
      const std::optional<std::string>& group_public_key,
      const std::optional<std::string>& group_private_key,
      const std::optional<cryptauthv2::ClientDirective>& client_directive) {
    SendSyncMetadataResponse(device_metadata_packets, group_public_key,
                             group_private_key, client_directive,
                             true /* is_first */);
  }

  void FailFirstSyncMetadataRequest(
      const NetworkRequestError& network_request_error) {
    ASSERT_TRUE(first_sync_metadata_failure_callback_);
    std::move(first_sync_metadata_failure_callback_).Run(network_request_error);
  }

  void VerifySecondSyncMetadataRequest(
      const std::string& expected_group_public_key,
      bool expected_need_group_private_key) {
    VerifySyncMetadataRequest(expected_group_public_key,
                              expected_need_group_private_key,
                              false /* is_first */);
  }

  void SendSecondSyncMetadataResponse(
      const std::vector<cryptauthv2::DeviceMetadataPacket>&
          device_metadata_packets,
      const std::optional<std::string>& group_public_key,
      const std::optional<std::string>& group_private_key,
      const std::optional<cryptauthv2::ClientDirective>& client_directive) {
    SendSyncMetadataResponse(device_metadata_packets, group_public_key,
                             group_private_key, client_directive,
                             false /* is_first */);
  }

  void FailSecondSyncMetadataRequest(
      const NetworkRequestError& network_request_error) {
    ASSERT_TRUE(second_sync_metadata_failure_callback_);
    std::move(second_sync_metadata_failure_callback_)
        .Run(network_request_error);
  }

  void VerifyMetadataSyncResult(
      const std::vector<cryptauthv2::DeviceMetadataPacket>&
          expected_device_metadata_packets,
      const std::optional<CryptAuthKey>& expected_new_group_key,
      const std::optional<std::string>& expected_group_private_key,
      const std::optional<cryptauthv2::ClientDirective>&
          expected_new_client_directive,
      CryptAuthDeviceSyncResult::ResultCode expected_result_code) {
    ASSERT_TRUE(device_sync_result_code_);
    EXPECT_EQ(expected_device_metadata_packets.size(),
              id_to_device_metadata_packet_map_.size());
    EXPECT_EQ(expected_new_group_key.has_value(), new_group_key_ != nullptr);
    EXPECT_EQ(expected_group_private_key.has_value(),
              encrypted_group_private_key_.has_value());
    EXPECT_EQ(expected_new_client_directive.has_value(),
              new_client_directive_.has_value());

    EXPECT_EQ(expected_result_code, *device_sync_result_code_);

    for (const cryptauthv2::DeviceMetadataPacket& expected_packet :
         expected_device_metadata_packets) {
      const auto it =
          id_to_device_metadata_packet_map_.find(expected_packet.device_id());
      EXPECT_TRUE(it != id_to_device_metadata_packet_map_.end());
      EXPECT_EQ(expected_packet.SerializeAsString(),
                it->second.SerializeAsString());
    }

    if (expected_new_group_key && new_group_key_)
      EXPECT_EQ(*expected_new_group_key, *new_group_key_);

    if (expected_group_private_key && encrypted_group_private_key_) {
      std::string decrypted_group_private_key = DecryptFakeEncryptedString(
          encrypted_group_private_key_->encrypted_private_key(),
          GetPrivateKeyFromPublicKeyForTest(
              GetLocalDeviceForTest().device_better_together_public_key));
      EXPECT_EQ(expected_group_private_key, decrypted_group_private_key);
    }

    if (expected_new_client_directive && new_client_directive_) {
      EXPECT_EQ(expected_new_client_directive->SerializeAsString(),
                new_client_directive_->SerializeAsString());
    }
  }

  base::MockOneShotTimer* timer() { return timer_; }

 private:
  void OnSyncMetadataResponse(const cryptauthv2::SyncMetadataRequest& request,
                              CryptAuthClient::SyncMetadataCallback callback,
                              CryptAuthClient::ErrorCallback error_callback) {
    ++num_sync_metadata_calls_;
    EXPECT_LE(num_sync_metadata_calls_, 2u);

    EXPECT_FALSE(second_sync_metadata_request_);
    EXPECT_FALSE(second_sync_metadata_success_callback_);
    EXPECT_FALSE(second_sync_metadata_failure_callback_);

    if (num_sync_metadata_calls_ == 1) {
      EXPECT_FALSE(first_sync_metadata_request_);
      EXPECT_FALSE(first_sync_metadata_success_callback_);
      EXPECT_FALSE(first_sync_metadata_failure_callback_);

      first_sync_metadata_request_ = request;
      first_sync_metadata_success_callback_ = std::move(callback);
      first_sync_metadata_failure_callback_ = std::move(error_callback);
    } else if (num_sync_metadata_calls_ == 2) {
      EXPECT_TRUE(first_sync_metadata_request_);
      EXPECT_FALSE(first_sync_metadata_success_callback_);
      EXPECT_TRUE(first_sync_metadata_failure_callback_);

      second_sync_metadata_request_ = request;
      second_sync_metadata_success_callback_ = std::move(callback);
      second_sync_metadata_failure_callback_ = std::move(error_callback);
    }
  }

  void VerifySyncMetadataRequest(const std::string& expected_group_public_key,
                                 bool expected_need_group_private_key,
                                 bool is_first) {
    if (is_first) {
      ASSERT_TRUE(first_sync_metadata_request_);
      EXPECT_TRUE(first_sync_metadata_success_callback_);
      EXPECT_TRUE(first_sync_metadata_failure_callback_);
    } else {
      ASSERT_TRUE(second_sync_metadata_request_);
      EXPECT_TRUE(second_sync_metadata_success_callback_);
      EXPECT_TRUE(second_sync_metadata_failure_callback_);
    }

    const cryptauthv2::SyncMetadataRequest& request =
        is_first ? *first_sync_metadata_request_
                 : *second_sync_metadata_request_;

    EXPECT_EQ(GetRequestContext().SerializeAsString(),
              request.context().SerializeAsString());
    EXPECT_EQ(expected_group_public_key, request.group_public_key());
    EXPECT_EQ(MakeFakeEncryptedString(
                  GetLocalDeviceForTest()
                      .better_together_device_metadata->SerializeAsString(),
                  expected_group_public_key),
              request.encrypted_metadata());
    EXPECT_EQ(expected_need_group_private_key,
              request.need_group_private_key());
    EXPECT_TRUE(request.freshness_token().empty());
  }

  void SendSyncMetadataResponse(
      const std::vector<cryptauthv2::DeviceMetadataPacket>&
          device_metadata_packets,
      const std::optional<std::string>& group_public_key,
      const std::optional<std::string>& group_private_key,
      const std::optional<cryptauthv2::ClientDirective>& client_directive,
      bool is_first) {
    cryptauthv2::SyncMetadataResponse response;
    *response.mutable_encrypted_metadata() = {device_metadata_packets.begin(),
                                              device_metadata_packets.end()};
    if (group_public_key)
      response.set_group_public_key(*group_public_key);

    if (group_private_key) {
      response.mutable_encrypted_group_private_key()->set_encrypted_private_key(
          MakeFakeEncryptedString(
              *group_private_key,
              GetLocalDeviceForTest().device_better_together_public_key));
    }

    if (client_directive)
      response.mutable_client_directive()->CopyFrom(*client_directive);

    if (is_first) {
      ASSERT_TRUE(first_sync_metadata_success_callback_);
      std::move(first_sync_metadata_success_callback_).Run(response);
    } else {
      ASSERT_TRUE(second_sync_metadata_success_callback_);
      std::move(second_sync_metadata_success_callback_).Run(response);
    }

    // The local device metadata sent in the SyncMetadata request should be
    // cached after a successful response.
    const cryptauthv2::SyncMetadataRequest& request =
        is_first ? *first_sync_metadata_request_
                 : *second_sync_metadata_request_;
    VerifyCachedLocalDeviceMetadata(
        request.encrypted_metadata(),
        DecryptFakeEncryptedString(
            request.encrypted_metadata(),
            GetPrivateKeyFromPublicKeyForTest(request.group_public_key())),
        request.group_public_key());
  }

  // Verify cached local device metadata, recorded after a successful
  // SyncMetadata response.
  void VerifyCachedLocalDeviceMetadata(
      const std::string& expected_encrypted_metadata,
      const std::string& expected_unencrypted_metadata,
      const std::string& expected_group_public_key) {
    EXPECT_EQ(expected_encrypted_metadata,
              util::DecodeFromString(pref_service_.GetString(
                  prefs::kCryptAuthLastSyncedEncryptedLocalDeviceMetadata)));
    EXPECT_EQ(expected_unencrypted_metadata,
              util::DecodeFromString(pref_service_.GetString(
                  prefs::kCryptAuthLastSyncedUnencryptedLocalDeviceMetadata)));
    EXPECT_EQ(expected_group_public_key,
              util::DecodeFromString(pref_service_.GetString(
                  prefs::kCryptAuthLastSyncedGroupPublicKey)));
  }

  void OnMetadataSyncComplete(
      const CryptAuthMetadataSyncer::IdToDeviceMetadataPacketMap&
          id_to_device_metadata_packet_map,
      std::unique_ptr<CryptAuthKey> new_group_key,
      const std::optional<cryptauthv2::EncryptedGroupPrivateKey>&
          encrypted_group_private_key,
      const std::optional<cryptauthv2::ClientDirective>& new_client_directive,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
    id_to_device_metadata_packet_map_ = id_to_device_metadata_packet_map;
    new_group_key_ = std::move(new_group_key);
    encrypted_group_private_key_ = encrypted_group_private_key;
    new_client_directive_ = new_client_directive;
    device_sync_result_code_ = device_sync_result_code;
  }

  FakeCryptAuthKeyCreator* key_creator() {
    return fake_cryptauth_key_creator_factory_->instance();
  }

  FakeCryptAuthEciesEncryptor* encryptor() {
    return fake_cryptauth_ecies_encryptor_factory_->instance();
  }

  size_t num_sync_metadata_calls_ = 0;

  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;

  std::unique_ptr<FakeCryptAuthKeyCreatorFactory>
      fake_cryptauth_key_creator_factory_;
  std::unique_ptr<FakeCryptAuthEciesEncryptorFactory>
      fake_cryptauth_ecies_encryptor_factory_;

  std::optional<cryptauthv2::SyncMetadataRequest> first_sync_metadata_request_;
  std::optional<cryptauthv2::SyncMetadataRequest> second_sync_metadata_request_;
  CryptAuthClient::SyncMetadataCallback first_sync_metadata_success_callback_;
  CryptAuthClient::SyncMetadataCallback second_sync_metadata_success_callback_;
  CryptAuthClient::ErrorCallback first_sync_metadata_failure_callback_;
  CryptAuthClient::ErrorCallback second_sync_metadata_failure_callback_;

  CryptAuthMetadataSyncer::IdToDeviceMetadataPacketMap
      id_to_device_metadata_packet_map_;
  std::unique_ptr<CryptAuthKey> new_group_key_;
  std::optional<cryptauthv2::EncryptedGroupPrivateKey>
      encrypted_group_private_key_;
  std::optional<cryptauthv2::ClientDirective> new_client_directive_;
  std::optional<CryptAuthDeviceSyncResult::ResultCode> device_sync_result_code_;

  std::optional<CryptAuthKey> initial_group_key_;
  std::unique_ptr<CryptAuthMetadataSyncer> metadata_syncer_;
};

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Success_FirstDeviceInDeviceSyncGroup) {
  // The first device in a group does not have an initial public or private key;
  // it is responsible for creating the group key.
  SyncMetadata(std::nullopt /* initial_group_public_key */,
               std::nullopt /* initial_group_private_key */);

  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  RunKeyCreator(true /* success */, group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets = {
      GetLocalDeviceMetadataPacketForTest()};
  SendFirstSyncMetadataResponse(device_metadata_packets, group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());
  VerifyMetadataSyncResult(
      device_metadata_packets,
      CryptAuthKey(group_public_key, group_private_key,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Success_InitialGroupKeyValid) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // The SyncMetadataResponse returns the same group public key sent in the
  // request, indicating that the initial group public key is still valid.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(GetAllTestDeviceMetadataPackets(),
                           std::nullopt /* expected_new_group_key */,
                           std::nullopt /* expected_group_private_key */,
                           cryptauthv2::GetClientDirectiveForTest(),
                           CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Success_InitialGroupPublicKeyValid_MissingInitialGroupPrivateKey) {
  // The device has already established the group public key with CryptAuth in a
  // previous SyncMetadata call, but never received the group private key. (This
  // can happen if the user's other devices haven't yet sent the group private
  // key to CryptAuth, encrypted for this device.) Now, this device is notified
  // that the private key is available and makes another SyncMetadata call.
  std::string group_public_key = kGroupPublicKey;
  SyncMetadata(group_public_key, std::nullopt /* group_private_key */);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 true /* expected_need_group_private_key */);

  // Response confirms that group public key is correct, and also sends
  // encrypted group private key.
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                group_public_key, group_private_key,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(GetAllTestDeviceMetadataPackets(),
                           std::nullopt /* expected_new_group_key */,
                           group_private_key,
                           cryptauthv2::GetClientDirectiveForTest(),
                           CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Success_InitialGroupKeyValid_UseCachedLocalDeviceMetadata) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);

  // Say the local device metadata was already encrypted with the with the
  // current group key and the device metadata hasn't changed.
  SetLocalDeviceMetadataPrefs(
      GetLocalDeviceForTest()
          .better_together_device_metadata->SerializeAsString(),
      group_public_key);

  SyncMetadata(group_public_key, group_private_key);

  // The encryptor does not need to be run because we are using cached metadata.

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // The SyncMetadataResponse returns the same group public key sent in the
  // request, indicating that the initial group public key is still valid.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(GetAllTestDeviceMetadataPackets(),
                           std::nullopt /* expected_new_group_key */,
                           std::nullopt /* expected_group_private_key */,
                           cryptauthv2::GetClientDirectiveForTest(),
                           CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(
    DeviceSyncCryptAuthMetadataSyncerImplTest,
    Success_InitialGroupKeyValid_DoNotUseCachedLocalDeviceMetadata_MetadataChanged) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);

  // Say the local device metadata was already encrypted with the current group
  // key but the device metadata is stale. Note: Include a non-UTF-8 character
  // to ensure that we handle them correctly in prefs.
  SetLocalDeviceMetadataPrefs("old_metadata\xC0", group_public_key);

  SyncMetadata(group_public_key, group_private_key);

  // Re-encryption occurs because the cached local device metadata is stale.
  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // The SyncMetadataResponse returns the same group public key sent in the
  // request, indicating that the initial group public key is still valid.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(GetAllTestDeviceMetadataPackets(),
                           std::nullopt /* expected_new_group_key */,
                           std::nullopt /* expected_group_private_key */,
                           cryptauthv2::GetClientDirectiveForTest(),
                           CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(
    DeviceSyncCryptAuthMetadataSyncerImplTest,
    Success_InitialGroupKeyValid_DoNotUseCachedLocalDeviceMetadata_GroupPublicKeyChanged) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);

  // Say the local device metadata was already encrypted with the current device
  // metadata but a stale group public key. Note: Include a non-UTF-8 character
  // to ensure that we handle them correctly in prefs.
  SetLocalDeviceMetadataPrefs(
      GetLocalDeviceForTest()
          .better_together_device_metadata->SerializeAsString(),
      "old_group_public_key\xC1");

  SyncMetadata(group_public_key, group_private_key);

  // Re-encryption occurs because the cached group public key is stale.
  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // The SyncMetadataResponse returns the same group public key sent in the
  // request, indicating that the initial group public key is still valid.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(GetAllTestDeviceMetadataPackets(),
                           std::nullopt /* expected_new_group_key */,
                           std::nullopt /* expected_group_private_key */,
                           cryptauthv2::GetClientDirectiveForTest(),
                           CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Success_InitialGroupKeyStale_CreateNewGroupKey) {
  std::string stale_group_public_key = kStaleGroupPublicKey;
  std::string stale_group_private_key =
      GetPrivateKeyFromPublicKeyForTest(stale_group_public_key);
  SyncMetadata(stale_group_public_key, stale_group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(stale_group_public_key,
                                          true /* succeed */);

  VerifyFirstSyncMetadataRequest(stale_group_public_key,
                                 false /* expected_need_group_private_key */);

  // An empty group_public_key in the response indicates that the local device
  // needs to generate the new group key pair.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                std::nullopt /* group_public_key */,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  RunKeyCreator(true /* success */, group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifySecondSyncMetadataRequest(group_public_key,
                                  false /* expected_need_group_private_key */);

  SendSecondSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                 group_public_key,
                                 std::nullopt /* group_private_key */,
                                 cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(
      GetAllTestDeviceMetadataPackets(),
      CryptAuthKey(group_public_key, group_private_key,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Success_InitialGroupKeyStale_GetNewGroupKeyFromCryptAuth) {
  std::string stale_group_public_key = kStaleGroupPublicKey;
  std::string stale_group_private_key =
      GetPrivateKeyFromPublicKeyForTest(stale_group_public_key);
  SyncMetadata(stale_group_public_key, stale_group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(stale_group_public_key,
                                          true /* succeed */);

  VerifyFirstSyncMetadataRequest(stale_group_public_key,
                                 false /* expected_need_group_private_key */);

  // The group public key in the response differs from the one sent in the
  // request, indicating that the local device should abandon its stale group
  // key in favor of the one from the response.
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  // The client now sends the correct group public key to CryptAuth in the
  // second request.
  VerifySecondSyncMetadataRequest(group_public_key,
                                  true /* expected_need_group_private_key */);

  // The second response includes the correct group public key and encrypted
  // group private key.
  SendSecondSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                 group_public_key, group_private_key,
                                 cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(
      GetAllTestDeviceMetadataPackets(),
      CryptAuthKey(group_public_key, std::string() /* group_private_key */,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Success_NoGroupPrivateKeySentByCryptAuth) {
  std::string stale_group_public_key = kStaleGroupPublicKey;
  std::string stale_group_private_key =
      GetPrivateKeyFromPublicKeyForTest(stale_group_public_key);
  SyncMetadata(stale_group_public_key, stale_group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(stale_group_public_key,
                                          true /* succeed */);

  VerifyFirstSyncMetadataRequest(stale_group_public_key,
                                 false /* expected_need_group_private_key */);

  std::string group_public_key = kGroupPublicKey;

  // CryptAuth sends the correct group public key pair in the response, but no
  // group private key.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifySecondSyncMetadataRequest(group_public_key,
                                  true /* expected_need_group_private_key */);

  // The group private key is not sent in the second response either. This can
  // happen if none of the user's other devices have encrypted and uploaded the
  // group private key to CryptAuth yet.
  SendSecondSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                 group_public_key,
                                 std::nullopt /* group_private_key */,
                                 cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(
      GetAllTestDeviceMetadataPackets(),
      CryptAuthKey(group_public_key, std::string() /* group_private_key */,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest, Failure_GroupKeyCreation) {
  // The first device in a group does not have an initial public or private key;
  // it is responsible for creating the group key.
  SyncMetadata(std::nullopt /* initial_group_public_key */,
               std::nullopt /* initial_group_private_key */);

  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  RunKeyCreator(false /* success */, group_public_key, group_private_key);

  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      std::nullopt /* expected_new_client_directive */,
      CryptAuthDeviceSyncResult::ResultCode::kErrorCreatingGroupKey);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest, Failure_MetadataEncryption) {
  std::string group_public_key = "corrupt_group_public_key";
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  // The local device metadata could not be encrypted with the provided group
  // public key, say because the key was corrupted.
  RunLocalBetterTogetherMetadataEncryptor(group_public_key,
                                          false /* succeed */);

  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      std::nullopt /* expected_new_client_directive */,
      CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingDeviceMetadata);
}

TEST_F(
    DeviceSyncCryptAuthMetadataSyncerImplTest,
    Failure_CannotEstablishGroupPublicKey_CryptAuthRequestsAnotherNewKeyPair) {
  std::string stale_group_public_key = kStaleGroupPublicKey;
  std::string stale_group_private_key =
      GetPrivateKeyFromPublicKeyForTest(stale_group_public_key);
  SyncMetadata(stale_group_public_key, stale_group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(stale_group_public_key,
                                          true /* succeed */);

  VerifyFirstSyncMetadataRequest(stale_group_public_key,
                                 false /* expected_need_group_private_key */);

  // An empty group_public_key in the response indicates that the local device
  // needs to generate a new group key pair.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                std::nullopt /* group_public_key */,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  RunKeyCreator(true /* success */, group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifySecondSyncMetadataRequest(group_public_key,
                                  false /* expected_need_group_private_key */);

  // The second SyncMetadataResponse asks the client to create a new key pair;
  // however, the v2 DeviceSync protocol states that it should take no more than
  // two SyncMetadata requests to establish the group public key.
  SendSecondSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                 std::nullopt /* group_public_key */,
                                 std::nullopt /* group_private_key */,
                                 cryptauthv2::GetClientDirectiveForTest());

  // The newly created group key is still returned. The next attempt will
  // indicate whether or not it is actually correct.
  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      CryptAuthKey(group_public_key, group_private_key,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kErrorEstablishingGroupPublicKey);
}

TEST_F(
    DeviceSyncCryptAuthMetadataSyncerImplTest,
    Failure_CannotEstablishGroupPublicKey_SecondResponseHasIncorrectKeyPair) {
  std::string stale_group_public_key = kStaleGroupPublicKey;
  std::string stale_group_private_key =
      GetPrivateKeyFromPublicKeyForTest(stale_group_public_key);
  SyncMetadata(stale_group_public_key, stale_group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(stale_group_public_key,
                                          true /* succeed */);

  VerifyFirstSyncMetadataRequest(stale_group_public_key,
                                 false /* expected_need_group_private_key */);

  // An empty group_public_key in the response indicates that the local device
  // needs to generate the new group key pair.
  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                std::nullopt /* group_public_key */,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  RunKeyCreator(true /* success */, group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifySecondSyncMetadataRequest(group_public_key,
                                  false /* expected_need_group_private_key */);

  // The group public key in second SyncMetadataResponse disagrees with that
  // sent in the request; however, the v2 DeviceSync protocol states that it
  // should take no more than two SyncMetadata requests to establish the group
  // public key.
  SendSecondSyncMetadataResponse(
      GetAllTestDeviceMetadataPackets(), stale_group_public_key,
      stale_group_private_key, cryptauthv2::GetClientDirectiveForTest());

  // The newly created group key and most recently returned encrypted group
  // private key are still returned. However, another SyncMetadata attempt will
  // still need to be made to establish the correct group key.
  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      CryptAuthKey(group_public_key, group_private_key,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      stale_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kErrorEstablishingGroupPublicKey);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_NoMetadataInResponse) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // No metadata is included in the response. At minimum, the local device's
  // metadata should be present.
  SendFirstSyncMetadataResponse({} /* device_metadata_packets */,
                                group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kErrorNoMetadataInResponse);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       FinishedWithNonFatalErrors_InvalidMetadataInResponse) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // One of the remote device's metadata in the response is invalid.
  // Specifially, the device name is empty.
  cryptauthv2::DeviceMetadataPacket invalid_packet =
      GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest();
  invalid_packet.set_device_name(std::string());
  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets = {
      GetLocalDeviceMetadataPacketForTest(),
      GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest(),
      invalid_packet};
  SendFirstSyncMetadataResponse(device_metadata_packets, group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  // The valid metadata is still returned.
  VerifyMetadataSyncResult(
      {GetLocalDeviceMetadataPacketForTest(),
       GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest()},
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_AllMetadataInResponseInvalid) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // All metadata in the response is invalid. Specifially, the device ID is
  // empty.
  cryptauthv2::DeviceMetadataPacket invalid_packet =
      GetLocalDeviceMetadataPacketForTest();
  invalid_packet.set_device_id(std::string());
  SendFirstSyncMetadataResponse({invalid_packet}, group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kErrorAllResponseMetadataInvalid);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       FinishedWithNonFatalErrors_DuplicateDeviceIdsInResponse) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // Two DeviceMetadataPackets with duplicate device IDs are returned in the
  // response.
  cryptauthv2::DeviceMetadataPacket duplicate_id_packet =
      GetLocalDeviceMetadataPacketForTest();
  duplicate_id_packet.set_device_name("duplicate_device_id: device_name");
  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets =
      GetAllTestDeviceMetadataPackets();
  device_metadata_packets.push_back(duplicate_id_packet);
  SendFirstSyncMetadataResponse(device_metadata_packets, group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  // Only the first metadata packet with a duplicate device ID is returned, the
  // other is discarded.
  VerifyMetadataSyncResult(
      GetAllTestDeviceMetadataPackets(),
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_NoLocalDeviceMetadataInResponse) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // The local device's metadata should always be included in the response.
  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets = {
      GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest(),
      GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest()};
  SendFirstSyncMetadataResponse(device_metadata_packets, group_public_key,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  VerifyMetadataSyncResult(
      {GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest(),
       GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest()},
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorNoLocalDeviceMetadataInResponse);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_Timeout_GroupKeyCreation) {
  SyncMetadata(std::nullopt /* initial_group_public_key */,
               std::nullopt /* initial_group_private_key */);

  // Timeout before group key creation completes.
  timer()->Fire();

  VerifyMetadataSyncResult({} /* expected_device_metadata_packets */,
                           std::nullopt /* expected_new_group_key */,
                           std::nullopt /* expected_group_private_key */,
                           std::nullopt /* expected_new_client_directive */,
                           CryptAuthDeviceSyncResult::ResultCode::
                               kErrorTimeoutWaitingForGroupKeyCreation);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_Timeout_LocalDeviceMetadataEncryption) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  // Timeout before local device metadata completes.
  timer()->Fire();

  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      std::nullopt /* expected_new_client_directive */,
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForLocalDeviceMetadataEncryption);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_Timeout_FirstSyncMetadataResponse) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // Timeout before first SyncMetadataResponse is received.
  timer()->Fire();

  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      std::nullopt /* expected_new_group_key */,
      std::nullopt /* expected_group_private_key */,
      std::nullopt /* expected_new_client_directive */,
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForFirstSyncMetadataResponse);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_Timeout_SecondSyncMetadataResponse) {
  std::string stale_group_public_key = kStaleGroupPublicKey;
  std::string stale_group_private_key =
      GetPrivateKeyFromPublicKeyForTest(stale_group_public_key);
  SyncMetadata(stale_group_public_key, stale_group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(stale_group_public_key,
                                          true /* succeed */);

  VerifyFirstSyncMetadataRequest(stale_group_public_key,
                                 false /* expected_need_group_private_key */);

  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                std::nullopt /* group_public_key */,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  RunKeyCreator(true /* success */, group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifySecondSyncMetadataRequest(group_public_key,
                                  false /* expected_need_group_private_key */);

  // Timeout before the second SyncMetadataResponse is received.
  timer()->Fire();

  // The new group key and client directive are still returned.
  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      CryptAuthKey(group_public_key, group_private_key,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForSecondSyncMetadataResponse);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_ApiCall_FirstSyncMetadata) {
  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  SyncMetadata(group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifyFirstSyncMetadataRequest(group_public_key,
                                 false /* expected_need_group_private_key */);

  // The first SyncMetadata API call fails with a HTTP 400 Bad Request error.
  FailFirstSyncMetadataRequest(NetworkRequestError::kBadRequest);

  VerifyMetadataSyncResult({} /* expected_device_metadata_packets */,
                           std::nullopt /* expected_new_group_key */,
                           std::nullopt /* expected_group_private_key */,
                           std::nullopt /* expected_new_client_directive */,
                           CryptAuthDeviceSyncResult::ResultCode::
                               kErrorSyncMetadataApiCallBadRequest);
}

TEST_F(DeviceSyncCryptAuthMetadataSyncerImplTest,
       Failure_ApiCall_SecondSyncMetadata) {
  std::string stale_group_public_key = kStaleGroupPublicKey;
  std::string stale_group_private_key =
      GetPrivateKeyFromPublicKeyForTest(stale_group_public_key);
  SyncMetadata(stale_group_public_key, stale_group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(stale_group_public_key,
                                          true /* succeed */);

  VerifyFirstSyncMetadataRequest(stale_group_public_key,
                                 false /* expected_need_group_private_key */);

  SendFirstSyncMetadataResponse(GetAllTestDeviceMetadataPackets(),
                                std::nullopt /* group_public_key */,
                                std::nullopt /* group_private_key */,
                                cryptauthv2::GetClientDirectiveForTest());

  std::string group_public_key = kGroupPublicKey;
  std::string group_private_key =
      GetPrivateKeyFromPublicKeyForTest(group_public_key);
  RunKeyCreator(true /* success */, group_public_key, group_private_key);

  RunLocalBetterTogetherMetadataEncryptor(group_public_key, true /* succeed */);

  VerifySecondSyncMetadataRequest(group_public_key,
                                  false /* expected_need_group_private_key */);

  // The second SyncMetadata API call fails with a HTTP 400 Bad Request error.
  FailSecondSyncMetadataRequest(NetworkRequestError::kBadRequest);

  // The new group key and client directive are still returned.
  VerifyMetadataSyncResult(
      {} /* expected_device_metadata_packets */,
      CryptAuthKey(group_public_key, group_private_key,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
      std::nullopt /* expected_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorSyncMetadataApiCallBadRequest);
}

}  // namespace device_sync

}  // namespace ash
