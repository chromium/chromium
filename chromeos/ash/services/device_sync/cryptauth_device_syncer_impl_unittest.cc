// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_syncer_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer.h"
#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_sync_test_devices.h"
#include "chromeos/ash/services/device_sync/fake_attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_ecies_encryptor.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_feature_status_getter.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_group_private_key_sharer.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_metadata_syncer.h"
#include "chromeos/ash/services/device_sync/fake_ecies_encryption.h"
#include "chromeos/ash/services/device_sync/fake_synced_bluetooth_address_tracker.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

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

const CryptAuthKey& GetStaleGroupKey() {
  static const base::NoDestructor<CryptAuthKey> stale_group_key([] {
    const char kStaleGroupPublicKey[] = "stale_group_public_key";
    return CryptAuthKey(kStaleGroupPublicKey,
                        GetPrivateKeyFromPublicKeyForTest(kStaleGroupPublicKey),
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256);
  }());
  return *stale_group_key;
}

const CryptAuthKey& GetGroupKeyWithoutPrivateKey() {
  static const base::NoDestructor<CryptAuthKey> group_key([] {
    return CryptAuthKey(kGroupPublicKey, std::string() /* private_key */,
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256);
  }());
  return *group_key;
}

}  // namespace

class DeviceSyncCryptAuthDeviceSyncerImplTest : public testing::Test {
 public:
  DeviceSyncCryptAuthDeviceSyncerImplTest(
      const DeviceSyncCryptAuthDeviceSyncerImplTest&) = delete;
  DeviceSyncCryptAuthDeviceSyncerImplTest& operator=(
      const DeviceSyncCryptAuthDeviceSyncerImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthDeviceSyncerImplTest()
      : client_factory_(std::make_unique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS)),
        fake_cryptauth_ecies_encryptor_factory_(
            std::make_unique<FakeCryptAuthEciesEncryptorFactory>()),
        fake_cryptauth_metadata_syncer_factory_(
            std::make_unique<FakeCryptAuthMetadataSyncerFactory>()),
        fake_cryptauth_feature_status_getter_factory_(
            std::make_unique<FakeCryptAuthFeatureStatusGetterFactory>()),
        fake_cryptauth_group_private_key_sharer_factory_(
            std::make_unique<FakeCryptAuthGroupPrivateKeySharerFactory>()),
        fake_attestation_certificates_syncer_(
            std::make_unique<FakeAttestationCertificatesSyncer>()),
        fake_synced_bluetooth_address_tracker_(
            std::make_unique<FakeSyncedBluetoothAddressTracker>()) {
    CryptAuthKeyRegistryImpl::RegisterPrefs(pref_service_.registry());
    key_registry_ = CryptAuthKeyRegistryImpl::Factory::Create(&pref_service_);
    CryptAuthDeviceRegistryImpl::RegisterPrefs(pref_service_.registry());
    device_registry_ =
        CryptAuthDeviceRegistryImpl::Factory::Create(&pref_service_);
  }

  ~DeviceSyncCryptAuthDeviceSyncerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    CryptAuthEciesEncryptorImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_ecies_encryptor_factory_.get());
    CryptAuthMetadataSyncerImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_metadata_syncer_factory_.get());
    CryptAuthFeatureStatusGetterImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_feature_status_getter_factory_.get());
    CryptAuthGroupPrivateKeySharerImpl::Factory::SetFactoryForTesting(
        fake_cryptauth_group_private_key_sharer_factory_.get());

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = mock_timer.get();

    fake_attestation_certificates_syncer_->SetIsUpdateRequired(true);

    syncer_ = CryptAuthDeviceSyncerImpl::Factory::Create(
        device_registry_.get(), key_registry_.get(), client_factory_.get(),
        fake_synced_bluetooth_address_tracker_.get(),
        fake_attestation_certificates_syncer_.get(), &pref_service_,
        std::move(mock_timer));

    std::string local_user_public_key =
        GetLocalDeviceForTest().better_together_device_metadata->public_key();
    key_registry_->AddKey(
        CryptAuthKeyBundle::Name::kUserKeyPair,
        CryptAuthKey(local_user_public_key,
                     GetPrivateKeyFromPublicKeyForTest(local_user_public_key),
                     CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
                     kCryptAuthFixedUserKeyPairHandle));

    std::string local_beto_public_key =
        GetLocalDeviceForTest().device_better_together_public_key;
    key_registry_->AddKey(
        CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether,
        CryptAuthKey(local_beto_public_key,
                     GetPrivateKeyFromPublicKeyForTest(local_beto_public_key),
                     CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
                     std::nullopt /* handle */));
  }

  // testing::Test:
  void TearDown() override {
    CryptAuthEciesEncryptorImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthMetadataSyncerImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthFeatureStatusGetterImpl::Factory::SetFactoryForTesting(nullptr);
    CryptAuthGroupPrivateKeySharerImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void CallSync() {
    syncer_->Sync(
        GetClientMetadata(), cryptauthv2::GetClientAppMetadataForTest(),
        base::BindOnce(
            &DeviceSyncCryptAuthDeviceSyncerImplTest::OnDeviceSyncComplete,
            base::Unretained(this)));
  }

  void SetDeviceRegistry(const std::vector<CryptAuthDevice>& devices) {
    CryptAuthDeviceRegistry::InstanceIdToDeviceMap map;
    for (const CryptAuthDevice& device : devices)
      map.insert_or_assign(device.instance_id(), device);

    device_registry_->SetRegistry(map);
  }

  void AddInitialGroupKeyToRegistry(const CryptAuthKey& group_key) {
    key_registry_->AddKey(
        CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey, group_key);

    VerifyGroupKeyInRegistry(group_key);
  }

  void VerifyGroupKeyInRegistry(const CryptAuthKey& group_key) {
    const CryptAuthKeyBundle* bundle = key_registry_->GetKeyBundle(
        CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey);

    ASSERT_TRUE(bundle);
    EXPECT_EQ(1u, bundle->handle_to_key_map().size());
    EXPECT_EQ(group_key, *bundle->GetActiveKey());
  }

  void VerifyMetadataSyncerInput(
      const CryptAuthKey* expected_initial_group_key) {
    EXPECT_EQ(client_factory_.get(),
              fake_cryptauth_metadata_syncer_factory_->last_client_factory());
    EXPECT_EQ(&pref_service_,
              fake_cryptauth_metadata_syncer_factory_->last_pref_service());
    ASSERT_EQ(1u, fake_cryptauth_metadata_syncer_factory_->instances().size());
    ASSERT_TRUE(metadata_syncer()->request_context());
    ASSERT_TRUE(metadata_syncer()->local_device_metadata());
    ASSERT_TRUE(metadata_syncer()->initial_group_key());

    EXPECT_EQ(GetRequestContext().SerializeAsString(),
              metadata_syncer()->request_context()->SerializeAsString());
    EXPECT_EQ(GetLocalDeviceForTest()
                  .better_together_device_metadata->SerializeAsString(),
              metadata_syncer()->local_device_metadata()->SerializeAsString());
    ASSERT_EQ(expected_initial_group_key == nullptr,
              metadata_syncer()->initial_group_key().value() == nullptr);
    if (expected_initial_group_key) {
      EXPECT_EQ(*expected_initial_group_key,
                *metadata_syncer()->initial_group_key().value());
    }
  }

  void VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus expected_pk_status,
      BetterTogetherMetadataStatus expected_metadata_status) {
    EXPECT_EQ(expected_pk_status, syncer_->group_private_key_status());
    EXPECT_EQ(expected_metadata_status,
              syncer_->better_together_metadata_status());
  }

  void FinishMetadataSyncerAttempt(
      const std::vector<cryptauthv2::DeviceMetadataPacket>& metadata_packets,
      const std::optional<CryptAuthKey>& new_group_key,
      const std::optional<std::string>& encrypted_group_private_key,
      const std::optional<cryptauthv2::ClientDirective> new_client_directive,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
    CryptAuthMetadataSyncer::IdToDeviceMetadataPacketMap
        id_to_device_metadata_packet_map;
    for (const cryptauthv2::DeviceMetadataPacket& packet : metadata_packets) {
      id_to_device_metadata_packet_map.insert_or_assign(packet.device_id(),
                                                        packet);
    }

    std::optional<cryptauthv2::EncryptedGroupPrivateKey> private_key;
    if (encrypted_group_private_key) {
      private_key = cryptauthv2::EncryptedGroupPrivateKey();
      private_key->set_encrypted_private_key(*encrypted_group_private_key);
    }
    std::unique_ptr<CryptAuthKey> new_group_key_ptr =
        new_group_key ? std::make_unique<CryptAuthKey>(*new_group_key)
                      : nullptr;
    metadata_syncer()->FinishAttempt(
        id_to_device_metadata_packet_map, std::move(new_group_key_ptr),
        private_key, new_client_directive, device_sync_result_code);
  }

  void VerifyFeatureStatusGetterInput(
      const base::flat_set<std::string>& expected_device_ids) {
    EXPECT_EQ(
        client_factory_.get(),
        fake_cryptauth_feature_status_getter_factory_->last_client_factory());
    ASSERT_EQ(
        1u, fake_cryptauth_feature_status_getter_factory_->instances().size());
    ASSERT_TRUE(feature_status_getter()->request_context());
    ASSERT_TRUE(feature_status_getter()->device_ids());

    EXPECT_EQ(GetRequestContext().SerializeAsString(),
              feature_status_getter()->request_context()->SerializeAsString());
    EXPECT_EQ(expected_device_ids, *feature_status_getter()->device_ids());
  }

  void FinishFeatureStatusGetterAttempt(
      const base::flat_set<std::string>& device_ids,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
    CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap
        id_to_device_software_feature_info_map;
    for (const std::string& id : device_ids) {
      id_to_device_software_feature_info_map.try_emplace(
          id, GetTestDeviceWithId(id).feature_states,
          GetTestDeviceWithId(id).last_update_time);
    }

    feature_status_getter()->FinishAttempt(
        id_to_device_software_feature_info_map, device_sync_result_code);
  }

  void RunGroupPrivateKeyDecryptor(
      const std::string& expected_encrypted_group_private_key,
      bool succeed) {
    ASSERT_TRUE(encryptor());
    ASSERT_EQ(1u, encryptor()->id_to_input_map().size());

    const auto it = encryptor()->id_to_input_map().begin();
    EXPECT_EQ(expected_encrypted_group_private_key, it->second.payload);

    std::string local_beto_private_key = GetPrivateKeyFromPublicKeyForTest(
        GetLocalDeviceForTest().device_better_together_public_key);
    EXPECT_EQ(local_beto_private_key, it->second.key);

    std::optional<std::string> decrypted_key;
    if (succeed) {
      decrypted_key =
          DecryptFakeEncryptedString(it->second.payload, it->second.key);
    }

    encryptor()->FinishAttempt(FakeCryptAuthEciesEncryptor::Action::kDecryption,
                               {{it->first, decrypted_key}});
  }

  // Fail decryption for IDs in |device_ids_to_fail|.
  void RunDeviceMetadataDecryptor(
      const std::vector<cryptauthv2::DeviceMetadataPacket>&
          expected_device_metadata_packets,
      const std::string& expected_unencrypted_group_private_key,
      const base::flat_set<std::string>& device_ids_to_fail) {
    ASSERT_TRUE(encryptor());

    CryptAuthEciesEncryptor::IdToInputMap id_to_encrypted_metadata_map;
    CryptAuthEciesEncryptor::IdToOutputMap id_to_unencrypted_metadata_map;
    for (const cryptauthv2::DeviceMetadataPacket& metadata :
         expected_device_metadata_packets) {
      id_to_encrypted_metadata_map[metadata.device_id()] =
          CryptAuthEciesEncryptor::PayloadAndKey(
              metadata.encrypted_metadata(),
              expected_unencrypted_group_private_key);

      id_to_unencrypted_metadata_map[metadata.device_id()] =
          base::Contains(device_ids_to_fail, metadata.device_id())
              ? std::nullopt
              : std::make_optional<std::string>(DecryptFakeEncryptedString(
                    metadata.encrypted_metadata(),
                    expected_unencrypted_group_private_key));
    }

    EXPECT_EQ(id_to_encrypted_metadata_map, encryptor()->id_to_input_map());

    encryptor()->FinishAttempt(FakeCryptAuthEciesEncryptor::Action::kDecryption,
                               id_to_unencrypted_metadata_map);
  }

  void VerifyGroupPrivateKeySharerInput(
      const CryptAuthKey& expected_group_key,
      const base::flat_set<std::string>& expected_device_ids) {
    EXPECT_EQ(client_factory_.get(),
              fake_cryptauth_group_private_key_sharer_factory_
                  ->last_client_factory());
    ASSERT_EQ(
        1u,
        fake_cryptauth_group_private_key_sharer_factory_->instances().size());
    ASSERT_TRUE(group_private_key_sharer()->request_context());
    ASSERT_TRUE(group_private_key_sharer()->group_key());

    EXPECT_EQ(
        GetRequestContext().SerializeAsString(),
        group_private_key_sharer()->request_context()->SerializeAsString());
    EXPECT_EQ(expected_group_key, *group_private_key_sharer()->group_key());

    CryptAuthGroupPrivateKeySharer::IdToEncryptingKeyMap
        expected_id_to_encrypting_key_map;
    for (const std::string& id : expected_device_ids) {
      expected_id_to_encrypting_key_map.insert_or_assign(
          id, GetTestDeviceWithId(id).device_better_together_public_key);
    }
    EXPECT_EQ(expected_id_to_encrypting_key_map,
              *group_private_key_sharer()->id_to_encrypting_key_map());
  }

  void FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
    group_private_key_sharer()->FinishAttempt(device_sync_result_code);
  }

  void VerifyDeviceSyncResult(
      const CryptAuthDeviceSyncResult& expected_result,
      const std::vector<CryptAuthDevice>& expected_devices_in_registry) {
    ASSERT_TRUE(device_sync_result_);
    EXPECT_EQ(expected_result, *device_sync_result_);

    if (expected_result.IsSuccess()) {
      EXPECT_EQ(kDefaultLocalDeviceBluetoothAddress,
                fake_synced_bluetooth_address_tracker_
                    ->last_synced_bluetooth_address());
    } else {
      EXPECT_TRUE(fake_synced_bluetooth_address_tracker_
                      ->last_synced_bluetooth_address()
                      .empty());
    }

    CryptAuthDeviceRegistry::InstanceIdToDeviceMap expected_registry;
    for (const CryptAuthDevice& device : expected_devices_in_registry) {
      expected_registry.insert_or_assign(device.instance_id(), device);
    }
    EXPECT_EQ(expected_registry, device_registry_->instance_id_to_device_map());
  }

  void VerifyNumberOfSuccessfulAttestationSyncCalls(int number_of_calls) {
    EXPECT_EQ(number_of_calls, fake_attestation_certificates_syncer_
                                   ->number_of_set_last_sync_timestamp_calls());
  }

  CryptAuthKeyRegistry* key_registry() { return key_registry_.get(); }

  base::MockOneShotTimer* timer() { return timer_; }

  base::test::ScopedFeatureList feature_list_;

 private:
  FakeCryptAuthEciesEncryptor* encryptor() {
    return fake_cryptauth_ecies_encryptor_factory_->instance();
  }

  FakeCryptAuthMetadataSyncer* metadata_syncer() {
    return fake_cryptauth_metadata_syncer_factory_->instances().back();
  }

  FakeCryptAuthFeatureStatusGetter* feature_status_getter() {
    return fake_cryptauth_feature_status_getter_factory_->instances().back();
  }

  FakeCryptAuthGroupPrivateKeySharer* group_private_key_sharer() {
    return fake_cryptauth_group_private_key_sharer_factory_->instances().back();
  }

  void OnDeviceSyncComplete(CryptAuthDeviceSyncResult device_sync_result) {
    device_sync_result_ = device_sync_result;
  }

  std::unique_ptr<MockCryptAuthClientFactory> client_factory_;
  std::unique_ptr<FakeCryptAuthEciesEncryptorFactory>
      fake_cryptauth_ecies_encryptor_factory_;
  std::unique_ptr<FakeCryptAuthMetadataSyncerFactory>
      fake_cryptauth_metadata_syncer_factory_;
  std::unique_ptr<FakeCryptAuthFeatureStatusGetterFactory>
      fake_cryptauth_feature_status_getter_factory_;
  std::unique_ptr<FakeCryptAuthGroupPrivateKeySharerFactory>
      fake_cryptauth_group_private_key_sharer_factory_;

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;
  std::unique_ptr<CryptAuthDeviceRegistry> device_registry_;
  std::unique_ptr<FakeAttestationCertificatesSyncer>
      fake_attestation_certificates_syncer_;
  std::unique_ptr<FakeSyncedBluetoothAddressTracker>
      fake_synced_bluetooth_address_tracker_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;

  std::optional<CryptAuthDeviceSyncResult> device_sync_result_;

  std::unique_ptr<CryptAuthDeviceSyncer> syncer_;
};

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       Success_FirstDeviceInDeviceSyncGroup) {
  CallSync();

  // Because the local device is the first device joining the group, there is no
  // group key yet. Also, no encrypted group private key is returned but we
  // already have the unencrypted group private key.
  VerifyMetadataSyncerInput(nullptr /* expected_initial_group_key */);
  FinishMetadataSyncerAttempt({GetLocalDeviceMetadataPacketForTest()},
                              GetGroupKey() /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKey());

  base::flat_set<std::string> device_ids = {
      GetLocalDeviceForTest().instance_id()};
  VerifyFeatureStatusGetterInput(device_ids);
  FinishFeatureStatusGetterAttempt(
      device_ids, CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);

  // Skip right to metadata decryption since an encrypted group private key was
  // not provided in the SyncMetadata response but we have the unencrypted group
  // private key in the key registry.
  RunDeviceMetadataDecryptor({GetLocalDeviceMetadataPacketForTest()},
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(GetGroupKey(), device_ids);
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      {GetLocalDeviceForTest()});
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest, Success_InitialGroupKeyValid) {
  // Add the correct group key to the registry.
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  // The initial group key is valid, so a new group key was not created.
  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(), std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // Even though we have the unencrypted group private key in the key registry,
  // we decrypt the group private key from CryptAuth and check consistency.
  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, true /* succeed */);

  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       Success_InitialGroupKeyValid_SomeDevicesHaveNoEncryptedMetadata) {
  // Add the correct group key to the registry.
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  // In addition to the local device, remote devices are returned that do not
  // have any encrypted metadata. This can happen if the remote device has not
  // uploaded metadata encrypted with the correct group public key.
  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets =
      GetAllTestDeviceMetadataPackets();
  device_metadata_packets[1].clear_encrypted_metadata();
  device_metadata_packets[2].clear_encrypted_metadata();

  // The initial group key is valid, so a new group key was not created.
  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      device_metadata_packets, std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // Even though we have the unencrypted group private key in the key registry,
  // we decrypt the group private key from CryptAuth and check consistency.
  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, true /* succeed */);

  // We should only attempt to decrypt the local device metadata because the
  // remote devices did not return any encrypted metadata.
  RunDeviceMetadataDecryptor({GetLocalDeviceMetadataPacketForTest()},
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // The remote devices did not send encrypted metadata.
  std::vector<CryptAuthDevice> devices = GetAllTestDevices();
  devices[1].better_together_device_metadata.reset();
  devices[2].better_together_device_metadata.reset();

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      devices);
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       Success_InitialGroupKeyValid_NoDevicesNeedGroupPrivateKey) {
  // Add the correct group key to the registry.
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  // Only return the local device metadata, noting that it does not need the
  // group private key. So, there is no need to share the group private key.
  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets = {
      ConvertTestDeviceToMetadataPacket(GetLocalDeviceForTest(),
                                        kGroupPublicKey,
                                        false /* need_group_private_key */)};
  VerifyMetadataSyncerInput(&GetGroupKey());

  // The initial group key is valid, so a new group key was not created.
  FinishMetadataSyncerAttempt(
      device_metadata_packets, std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKey());

  base::flat_set<std::string> device_ids = {
      GetLocalDeviceForTest().instance_id()};
  VerifyFeatureStatusGetterInput(device_ids);
  FinishFeatureStatusGetterAttempt(
      device_ids, CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // Even though we have the unencrypted group private key in the key registry,
  // we decrypt the group private key from CryptAuth and check consistency.
  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, true /* succeed */);

  RunDeviceMetadataDecryptor(device_metadata_packets,
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      {GetLocalDeviceForTest()});
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       Success_InitialGroupPublicKeyValid_NeedGroupPrivateKey) {
  // Add the correct group public key to the registry. Note that we still need
  // the group private key from CryptAuth.
  AddInitialGroupKeyToRegistry(GetGroupKeyWithoutPrivateKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  // The initial group public key is valid, so a new group key was not created;
  // however, we now receive the group private key from CryptAuth.
  VerifyMetadataSyncerInput(&GetGroupKeyWithoutPrivateKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(), std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKeyWithoutPrivateKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // The new group private key received from CryptAuth is decrypted, bundled
  // with the existing group public key, and added to the key registry.
  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, true /* succeed */);

  VerifyGroupKeyInRegistry(GetGroupKey());

  // Since we now have the decrypted group private key, device metadata can be
  // decrypted.
  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       Success_InitialGroupKeyStale_CreateNewGroupKey) {
  AddInitialGroupKeyToRegistry(GetStaleGroupKey());

  CallSync();

  // The initial group key is stale, so CryptAuth instructs us to create the new
  // group key. No encrypted private key is returned but we own the unencrypted
  // group private key.
  VerifyMetadataSyncerInput(&GetStaleGroupKey());
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              GetGroupKey() /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKey());
  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // Decrypt metadata since we own the group private key.
  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(
    DeviceSyncCryptAuthDeviceSyncerImplTest,
    Success_InitialGroupKeyStale_GetNewGroupPublicKeyFromCryptAuth_WithGroupPrivateKey) {
  AddInitialGroupKeyToRegistry(GetStaleGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  // The initial group key is stale, so CryptAuth provides us with the new
  // unencrypted group public key and encrypted group private key.
  VerifyMetadataSyncerInput(&GetStaleGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(),
      GetGroupKeyWithoutPrivateKey() /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // The group private key will be added after it is decrypted.
  VerifyGroupKeyInRegistry(GetGroupKeyWithoutPrivateKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, true /* succeed */);
  VerifyGroupKeyInRegistry(GetGroupKey());

  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(
    DeviceSyncCryptAuthDeviceSyncerImplTest,
    Success_InitialGroupKeyStale_GetNewGroupPublicKeyFromCryptAuth_WithoutGroupPrivateKey) {
  AddInitialGroupKeyToRegistry(GetStaleGroupKey());

  CallSync();

  // The initial group key is stale, so CryptAuth provides us with the new
  // unencrypted group public key but no encrypted group private key. This can
  // happen if the other devices have not shared their encrypted group private
  // key with CryptAuth yet.
  VerifyMetadataSyncerInput(&GetStaleGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(),
      GetGroupKeyWithoutPrivateKey() /* new_group_key */,
      std::nullopt /* encrypted_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKeyWithoutPrivateKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kGroupPrivateKeyMissing);

  // Only the local device has its BetterTogetherDeviceMetadata in the device
  // registry since the other metadata cannot be decrypted without the group
  // private key, and because the previous device registry did not have any
  // existing metadata to draw from.
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       Success_PreserveExistingBetterTogetherMetadata) {
  // Populate existing registry with all devices and their BetterTogether
  // metadata.
  SetDeviceRegistry(GetAllTestDevices());

  AddInitialGroupKeyToRegistry(GetStaleGroupKey());

  CallSync();

  // The initial group key is stale, so CryptAuth provides us with the new
  // unencrypted group public key but no encrypted group private key. This can
  // happen if the other devices have not shared their encrypted group private
  // key with CryptAuth yet.
  VerifyMetadataSyncerInput(&GetStaleGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(),
      GetGroupKeyWithoutPrivateKey() /* new_group_key */,
      std::nullopt /* encrypted_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKeyWithoutPrivateKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kGroupPrivateKeyMissing);

  // Even though the new device BetterTogether metadata could not be decrypted,
  // the new registry should preserve the BetterTogether metadata from the
  // previous registry.
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::kSuccess,
                                false /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_FromMetadataSyncer) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  // Say the remote device metadata was invalid and not returned. Aside from the
  // result code, everything continues as if only the local device was in the
  // DeviceSync group.
  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      {GetLocalDeviceMetadataPacketForTest()}, std::nullopt /* new_group_key */,
      std::nullopt /* encrypted_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);

  base::flat_set<std::string> device_ids = {
      GetLocalDeviceForTest().instance_id()};
  VerifyFeatureStatusGetterInput(device_ids);
  FinishFeatureStatusGetterAttempt(
      device_ids, CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunDeviceMetadataDecryptor({GetLocalDeviceMetadataPacketForTest()},
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(GetGroupKey(), device_ids);
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      {GetLocalDeviceForTest()});
}

TEST_F(
    DeviceSyncCryptAuthDeviceSyncerImplTest,
    NonFatalError_InitialGroupKeyStale_GetNewGroupPublicKeyFromCryptAuth_WithEmptyGroupPrivateKey) {
  AddInitialGroupKeyToRegistry(GetStaleGroupKey());

  CallSync();

  // The initial group key is stale, so CryptAuth provides us with the new
  // unencrypted group public key but an unexpectedly empty encrypted group
  // private key string. This is considered a non-fatal error.
  VerifyMetadataSyncerInput(&GetStaleGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(),
      GetGroupKeyWithoutPrivateKey() /* new_group_key */,
      std::string() /* encrypted_group_private_key */,
      cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKeyWithoutPrivateKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kEncryptedGroupPrivateKeyEmpty,
      BetterTogetherMetadataStatus::kGroupPrivateKeyMissing);

  // Only the local device has its BetterTogetherDeviceMetadata in the device
  // registry since the other metadata cannot be decrypted without the group
  // private key, and because the previous device registry did not have any
  // existing metadata to draw from.
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_FromFeatureStatusGetter_MissingDeviceFeatureStatuses) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // The feature statuses are missing for a remote device, so it will not be
  // added to the registry.
  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      {GetLocalDeviceForTest().instance_id(),
       GetRemoteDeviceHasGroupPrivateKeyForTest().instance_id()},
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);

  RunDeviceMetadataDecryptor(
      {GetLocalDeviceMetadataPacketForTest(),
       GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest()},
      GetGroupKey().private_key(), {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      {GetLocalDeviceForTest(), GetRemoteDeviceHasGroupPrivateKeyForTest()});
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_GroupPrivateKeyDisagreement) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());

  // CryptAuth's group public key agrees with our local storage but the group
  // private key differs. We continue using our local group private key and hope
  // for the best.
  std::string wrong_encrypted_group_private_key = MakeFakeEncryptedString(
      GetStaleGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              std::nullopt /* new_group_key */,
                              wrong_encrypted_group_private_key,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunGroupPrivateKeyDecryptor(wrong_encrypted_group_private_key,
                              true /* succeed */);
  VerifyGroupKeyInRegistry(GetGroupKey());

  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_NoDevicesHaveEncryptedMetadata) {
  // Add the correct group key to the registry.
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  // All metadata packets are missing encrypted metadata, though we always
  // expect the local device to have encrypted metadata, at a minimum.
  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets =
      GetAllTestDeviceMetadataPackets();
  for (auto& packet : device_metadata_packets)
    packet.clear_encrypted_metadata();

  // The initial group key is valid, so a new group key was not created.
  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      device_metadata_packets, std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupKeyInRegistry(GetGroupKey());

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // Even though we have the unencrypted group private key in the key registry,
  // we decrypt the group private key from CryptAuth and check consistency.
  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, true /* succeed */);

  // The metadata decryptor will not be run because there is no metadata to
  // decrypt.
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kEncryptedMetadataEmpty);
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // The local device is still able to populate its own metadata.
  std::vector<CryptAuthDevice> devices = GetAllTestDevices();
  devices[1].better_together_device_metadata.reset();
  devices[2].better_together_device_metadata.reset();

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      devices);
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_MetadataDecryptionFailed) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // Fail metadata decryption.
  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             GetAllTestDeviceIds() /* device_ids_to_fail */);

  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_MetadataParsingFailed) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());

  // Replace the serialized BetterTogetherDeviceMetadata protos with a string
  // that cannot be interpreted as a proto, resulting in a parsing error.
  std::vector<cryptauthv2::DeviceMetadataPacket> corrupt_metadata_packets =
      GetAllTestDeviceMetadataPackets();
  for (cryptauthv2::DeviceMetadataPacket& packet : corrupt_metadata_packets) {
    *packet.mutable_encrypted_metadata() = MakeFakeEncryptedString(
        "Not a BetterTogetherDeviceMetadata proto", GetGroupKey().public_key());
  }

  FinishMetadataSyncerAttempt(corrupt_metadata_packets,
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunDeviceMetadataDecryptor(corrupt_metadata_packets,
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_InconsistentLocalDeviceMetadata) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());

  // The local device's DeviceMetadataPacket from CryptAuth that differs from
  // our local device metadata. The local device metadata from CryptAuth is
  // never used expect for a sanity check. If the local device metadata is
  // inconsistent, an error message is printed.
  cryptauthv2::BetterTogetherDeviceMetadata corrupt_beto_metadata;
  corrupt_beto_metadata.set_no_pii_device_name("corrupt_device_name");
  cryptauthv2::DeviceMetadataPacket corrupt_local_device_packet =
      GetLocalDeviceMetadataPacketForTest();
  corrupt_local_device_packet.set_encrypted_metadata(MakeFakeEncryptedString(
      corrupt_beto_metadata.SerializeAsString(), kGroupPublicKey));

  std::vector<cryptauthv2::DeviceMetadataPacket> device_metadata_packets = {
      corrupt_local_device_packet,
      GetRemoteDeviceMetadataPacketNeedsGroupPrivateKeyForTest(),
      GetRemoteDeviceMetadataPacketHasGroupPrivateKeyForTest()};
  FinishMetadataSyncerAttempt(device_metadata_packets,
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunDeviceMetadataDecryptor(device_metadata_packets,
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       NonFatalError_FromGroupPrivateKeySharer) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  // Group private key sharer finished with non-fatal errors. This should only
  // affect the final result code.
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest, FatalError_MissingUserKeyPair) {
  key_registry()->DeleteKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                            kCryptAuthFixedUserKeyPairHandle);

  CallSync();

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::kErrorMissingUserKeyPair,
          false /* device_registry_changed */,
          std::nullopt /* client_directive */),
      {} /* expected_devices_in_registry */);
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest, FatalError_FromMetadataSyncer) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt({} /* metadata_packets */,
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::
                                  kErrorSyncMetadataApiCallBadRequest);

  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                    kErrorSyncMetadataApiCallBadRequest,
                                false /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      {} /* expected_device_in_registry */);
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       FatalError_MissingLocalDeviceFeatureStatuses) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // The feature statuses are missing for the local device.
  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      {GetRemoteDeviceHasGroupPrivateKeyForTest().instance_id(),
       GetRemoteDeviceNeedsGroupPrivateKeyForTest().instance_id()},
      CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kWaitingForGroupPrivateKey,
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                    kErrorMissingLocalDeviceFeatureStatuses,
                                false /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      {} /* expected_device_in_registry */);
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       FatalError_FromFeatureStatusGetter) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // The feature statuses are missing for the local device.
  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(),
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorBatchGetFeatureStatusesApiCallBadRequest);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kWaitingForGroupPrivateKey,
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);
  VerifyDeviceSyncResult(CryptAuthDeviceSyncResult(
                             CryptAuthDeviceSyncResult::ResultCode::
                                 kErrorBatchGetFeatureStatusesApiCallBadRequest,
                             false /* device_registry_changed */,
                             cryptauthv2::GetClientDirectiveForTest()),
                         {} /* expected_device_in_registry */);
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       FatalError_MissingLocalDeviceSyncBetterTogetherKey) {
  // Our DeviceSync:BetterTogether key is missing from the registry, so the
  // group private key cannot be decrypted.
  key_registry()->DeleteKey(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether,
      key_registry()
          ->GetActiveKey(CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether)
          ->handle());

  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(), std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kLocalDeviceSyncBetterTogetherKeyMissing,
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);
  VerifyDeviceSyncResult(CryptAuthDeviceSyncResult(
                             CryptAuthDeviceSyncResult::ResultCode::
                                 kErrorMissingLocalDeviceSyncBetterTogetherKey,
                             true /* device_registry_changed */,
                             cryptauthv2::GetClientDirectiveForTest()),
                         GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       FatalError_DecryptingGroupPrivateKey) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(), std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  // Fail group private key decryption.
  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, false /* succeed */);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeyDecryptionFailed,
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                    kErrorDecryptingGroupPrivateKey,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       FatalError_FromGroupPrivateKeySharer) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(GetAllTestDeviceMetadataPackets(),
                              std::nullopt /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunDeviceMetadataDecryptor(GetAllTestDeviceMetadataPackets(),
                             GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);

  // Group private key sharer finished with fatal errors. This should only
  // affect the final result code.
  VerifyGroupPrivateKeySharerInput(
      GetGroupKey(), GetAllTestDeviceIdsThatNeedGroupPrivateKey());
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallBadRequest);

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                    kErrorShareGroupPrivateKeyApiCallBadRequest,
                                true /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevices());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       FatalError_Timeout_GroupPrivateKeyDecryption) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(), std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  timer()->Fire();

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kWaitingForGroupPrivateKey,
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::
              kErrorTimeoutWaitingForGroupPrivateKeyDecryption,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       FatalError_Timeout_DeviceMetadataDecryption) {
  AddInitialGroupKeyToRegistry(GetGroupKey());

  CallSync();

  std::string encrypted_group_private_key = MakeFakeEncryptedString(
      GetGroupKey().private_key(),
      GetLocalDeviceForTest().device_better_together_public_key);

  VerifyMetadataSyncerInput(&GetGroupKey());
  FinishMetadataSyncerAttempt(
      GetAllTestDeviceMetadataPackets(), std::nullopt /* new_group_key */,
      encrypted_group_private_key, cryptauthv2::GetClientDirectiveForTest(),
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyFeatureStatusGetterInput(GetAllTestDeviceIds());
  FinishFeatureStatusGetterAttempt(
      GetAllTestDeviceIds(), CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  RunGroupPrivateKeyDecryptor(encrypted_group_private_key, true /* succeed */);
  VerifyGroupKeyInRegistry(GetGroupKey());

  timer()->Fire();

  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kGroupPrivateKeySuccessfullyDecrypted,
      BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata);
  VerifyDeviceSyncResult(
      CryptAuthDeviceSyncResult(
          CryptAuthDeviceSyncResult::ResultCode::
              kErrorTimeoutWaitingForDeviceMetadataDecryption,
          true /* device_registry_changed */,
          cryptauthv2::GetClientDirectiveForTest()),
      GetAllTestDevicesWithoutRemoteMetadata());
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       LastSyncTimestampNotSetIfEcheDisabled) {
  feature_list_.InitWithFeatures(/* enabled_features= */ {},
                                 /* disabled_features= */ {features::kEcheSWA});

  CryptAuthDevice device = GetLocalDeviceForTest();
  device.feature_states[multidevice::SoftwareFeature::kEcheHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  cryptauthv2::AttestationData* attestation_data =
      device.better_together_device_metadata->mutable_attestation_data();
  attestation_data->set_type(
      cryptauthv2::AttestationData::CROS_SOFT_BIND_CERT_CHAIN);
  attestation_data->add_certificates(
      FakeAttestationCertificatesSyncer::kFakeCert);
  cryptauthv2::DeviceMetadataPacket packet =
      GetLocalDeviceMetadataPacketForTest(device);

  CallSync();
  FinishMetadataSyncerAttempt({packet}, GetGroupKey() /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  base::flat_set<std::string> device_ids = {device.instance_id()};
  FinishFeatureStatusGetterAttempt(
      device_ids, CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  RunDeviceMetadataDecryptor({packet}, GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyNumberOfSuccessfulAttestationSyncCalls(0);
}

TEST_F(DeviceSyncCryptAuthDeviceSyncerImplTest,
       LastSyncTimestampSetIfEcheEnabled) {
  feature_list_.InitWithFeatures(
      /* enabled_features= */ {features::kEcheSWA,
                               features::kCryptauthAttestationSyncing},
      /* disabled_features= */ {});

  CryptAuthDevice device = GetLocalDeviceForTest();
  device.feature_states[multidevice::SoftwareFeature::kEcheHost] =
      multidevice::SoftwareFeatureState::kEnabled;
  cryptauthv2::AttestationData* attestation_data =
      device.better_together_device_metadata->mutable_attestation_data();
  attestation_data->set_type(
      cryptauthv2::AttestationData::CROS_SOFT_BIND_CERT_CHAIN);
  attestation_data->add_certificates("certificate");
  cryptauthv2::DeviceMetadataPacket packet =
      GetLocalDeviceMetadataPacketForTest(device);

  CallSync();
  FinishMetadataSyncerAttempt({packet}, GetGroupKey() /* new_group_key */,
                              std::nullopt /* encrypted_group_private_key */,
                              cryptauthv2::GetClientDirectiveForTest(),
                              CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  base::flat_set<std::string> device_ids = {device.instance_id()};
  FinishFeatureStatusGetterAttempt(
      device_ids, CryptAuthDeviceSyncResult::ResultCode::kSuccess);
  RunDeviceMetadataDecryptor({packet}, GetGroupKey().private_key(),
                             {} /* device_ids_to_fail */);
  VerifyGroupPrivateKeyAndBetterTogetherMetadataStatus(
      GroupPrivateKeyStatus::kNoEncryptedGroupPrivateKeyReceived,
      BetterTogetherMetadataStatus::kMetadataDecrypted);
  FinishShareGroupPrivateKeyAttempt(
      CryptAuthDeviceSyncResult::ResultCode::kSuccess);

  VerifyNumberOfSuccessfulAttestationSyncCalls(1);
}

}  // namespace device_sync

}  // namespace ash
