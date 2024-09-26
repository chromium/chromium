// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/remote_device_provider_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_device_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_v2_device_manager.h"
#include "chromeos/ash/services/device_sync/fake_remote_device_v2_loader.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/ash/services/device_sync/remote_device_loader.h"
#include "chromeos/ash/services/device_sync/remote_device_v2_loader_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kTestUserEmail[] = "test@gmail.com";
const char kTestUserPrivateKey[] = "testUserPrivateKey";
const char kTestRemoteDeviceInstanceIdPrefix[] = "instanceId-";
const char kTestRemoteDeviceNamePrefix[] = "name-";
const char kTestRemoteDevicePiiFreeNamePrefix[] = "piiFreeName-";
const char kTestRemoteDevicePublicKeyPrefix[] = "publicKey-";
const char kTestRemoteDevicePskPrefix[] = "psk-";
const char kTestRemoteDeviceBluetoothPublicAddressPrefix[] = "address-";

multidevice::RemoteDevice CreateRemoteDeviceForTest(const std::string& suffix,
                                                    bool has_instance_id,
                                                    bool has_public_key) {
  DCHECK(has_instance_id || has_public_key);

  // Change BeaconSeed data depending on whether it has Instance ID or not.
  std::string beacon_seed_data =
      has_instance_id ? "has Instance ID" : "no Instance ID";

  return multidevice::RemoteDevice(
      kTestUserEmail,
      has_instance_id ? kTestRemoteDeviceInstanceIdPrefix + suffix
                      : std::string(),
      kTestRemoteDeviceNamePrefix + suffix,
      kTestRemoteDevicePiiFreeNamePrefix + suffix,
      has_public_key ? kTestRemoteDevicePublicKeyPrefix + suffix
                     : std::string(),
      kTestRemoteDevicePskPrefix + suffix, 100L /* last_update_time_millis */,
      {} /* software_features */,
      {multidevice::BeaconSeed(
          beacon_seed_data, base::Time::FromMillisecondsSinceUnixEpoch(200L),
          base::Time::FromMillisecondsSinceUnixEpoch(300L))},
      kTestRemoteDeviceBluetoothPublicAddressPrefix + suffix);
}

// Provide four fake RemoteDevices associated with a v1 DeviceSync. These
// devices have the IDs:
//   0. "" /* instance_id */ , "publicKey-0"
//   1. "" /* instance_id */ , "publicKey-1"
//   2. "" /* instance_id */ , "publicKey-2"
//   3. "" /* instance_id */ , "publicKey-v1-only"
const multidevice::RemoteDeviceList& GetV1RemoteDevices() {
  static const multidevice::RemoteDeviceList devices{
      CreateRemoteDeviceForTest("0", false /* has_instance_id */,
                                true /* has_public_key */),
      CreateRemoteDeviceForTest("1", false /* has_instance_id */,
                                true /* has_public_key */),
      CreateRemoteDeviceForTest("2", false /* has_instance_id */,
                                true /* has_public_key */),
      CreateRemoteDeviceForTest("v1-only", false /* has_instance_id */,
                                true /* has_public_key */),
  };
  return devices;
}

// Provide five fake RemoteDevices associated with a v2 DeviceSync. These
// devices have the IDs:
//   0. "instanceId-0", "publicKey-0"
//   1. "instanceId-1", "" /* public_key */
//   2. "instanceId-2", "publicKey-2"
//   3. "instanceId-v2-only", "publicKey-v2-only"
//   4. "instanceId-v2-only_no-public-key", "" /* public_key */
const multidevice::RemoteDeviceList& GetV2RemoteDevices() {
  static const multidevice::RemoteDeviceList devices{
      CreateRemoteDeviceForTest("0", true /* has_instance_id */,
                                true /* has_public_key */),
      CreateRemoteDeviceForTest("1", true /* has_instance_id */,
                                false /* has_public_key */),
      CreateRemoteDeviceForTest("2", true /* has_instance_id */,
                                true /* has_public_key */),
      CreateRemoteDeviceForTest("v2-only", true /* has_instance_id */,
                                true /* has_public_key */),
      CreateRemoteDeviceForTest("v2-only_no-public-key",
                                true /* has_instance_id */,
                                false /* has_public_key */),
  };
  return devices;
}

CryptAuthDevice ConvertRemoteDeviceToCryptAuthDevice(
    const multidevice::RemoteDevice& remote_device) {
  cryptauthv2::BetterTogetherDeviceMetadata beto_device_metadata;
  beto_device_metadata.set_public_key(remote_device.public_key);
  beto_device_metadata.set_no_pii_device_name(remote_device.pii_free_name);
  for (const multidevice::BeaconSeed& seed : remote_device.beacon_seeds)
    *beto_device_metadata.add_beacon_seeds() = ToCryptAuthV2Seed(seed);

  return CryptAuthDevice(remote_device.instance_id, remote_device.name,
                         "DeviceSync:BetterTogether public key",
                         base::Time::FromMillisecondsSinceUnixEpoch(
                             remote_device.last_update_time_millis),
                         remote_device.public_key.empty()
                             ? std::nullopt
                             : std::make_optional(beto_device_metadata),
                         remote_device.software_features);
}

class TestObserver : public RemoteDeviceProvider::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  int num_times_device_list_changed() { return num_times_device_list_changed_; }

  // RemoteDeviceProvider::Observer:
  void OnSyncDeviceListChanged() override { num_times_device_list_changed_++; }

 private:
  int num_times_device_list_changed_ = 0;
};

}  // namespace

class FakeDeviceLoader final : public RemoteDeviceLoader {
 public:
  class TestRemoteDeviceLoaderFactory final
      : public RemoteDeviceLoader::Factory {
   public:
    TestRemoteDeviceLoaderFactory() = default;
    ~TestRemoteDeviceLoaderFactory() = default;

    std::unique_ptr<RemoteDeviceLoader> CreateInstance(
        const std::vector<cryptauth::ExternalDeviceInfo>& device_info_list,
        const std::string& user_email,
        const std::string& user_private_key,
        std::unique_ptr<multidevice::SecureMessageDelegate>
            secure_message_delegate) override {
      EXPECT_EQ(std::string(kTestUserEmail), user_email);
      EXPECT_EQ(std::string(kTestUserPrivateKey), user_private_key);
      std::unique_ptr<FakeDeviceLoader> device_loader =
          std::make_unique<FakeDeviceLoader>();
      device_loader->remote_device_loader_factory_ = this;
      return device_loader;
    }

    void InvokeLastCallback(
        const std::vector<cryptauth::ExternalDeviceInfo>& device_info_list) {
      ASSERT_TRUE(!callback_.is_null());
      // Fetch only the devices inserted by tests, since GetV1RemoteDevices()
      // contains all available devices.
      multidevice::RemoteDeviceList devices;
      for (const auto& remote_device : GetV1RemoteDevices()) {
        for (const auto& external_device_info : device_info_list) {
          if (remote_device.public_key == external_device_info.public_key())
            devices.push_back(remote_device);
        }
      }
      std::move(callback_).Run(devices);
    }

    // Fetch is only started if the change result passed to OnSyncFinished() is
    // CHANGED and sync is SUCCESS.
    bool HasQueuedCallback() { return !callback_.is_null(); }

    void QueueCallback(RemoteDeviceCallback callback) {
      callback_ = std::move(callback);
    }

   private:
    RemoteDeviceLoader::RemoteDeviceCallback callback_;
  };

  FakeDeviceLoader()
      : RemoteDeviceLoader(std::vector<cryptauth::ExternalDeviceInfo>(),
                           "",
                           "",
                           nullptr) {}

  ~FakeDeviceLoader() override {}

  raw_ptr<TestRemoteDeviceLoaderFactory> remote_device_loader_factory_;

  void Load(RemoteDeviceCallback callback) override {
    remote_device_loader_factory_->QueueCallback(std::move(callback));
  }
};

class DeviceSyncRemoteDeviceProviderImplTest : public ::testing::Test {
 public:
  DeviceSyncRemoteDeviceProviderImplTest() = default;

  DeviceSyncRemoteDeviceProviderImplTest(
      const DeviceSyncRemoteDeviceProviderImplTest&) = delete;
  DeviceSyncRemoteDeviceProviderImplTest& operator=(
      const DeviceSyncRemoteDeviceProviderImplTest&) = delete;

  void SetUp() override {
    fake_device_manager_ = std::make_unique<FakeCryptAuthDeviceManager>();
    fake_v2_device_manager_ = std::make_unique<FakeCryptAuthV2DeviceManager>();

    fake_secure_message_delegate_factory_ =
        std::make_unique<multidevice::FakeSecureMessageDelegateFactory>();
    multidevice::SecureMessageDelegateImpl::Factory::SetFactoryForTesting(
        fake_secure_message_delegate_factory_.get());

    test_device_loader_factory_ =
        std::make_unique<FakeDeviceLoader::TestRemoteDeviceLoaderFactory>();
    RemoteDeviceLoader::Factory::SetFactoryForTesting(
        test_device_loader_factory_.get());
    fake_remote_device_v2_loader_factory_ =
        std::make_unique<FakeRemoteDeviceV2LoaderFactory>();
    RemoteDeviceV2LoaderImpl::Factory::SetFactoryForTesting(
        fake_remote_device_v2_loader_factory_.get());

    test_observer_ = std::make_unique<TestObserver>();
  }

  void TearDown() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetFactoryForTesting(
        nullptr);
    RemoteDeviceLoader::Factory::SetFactoryForTesting(nullptr);
    RemoteDeviceV2LoaderImpl::Factory::SetFactoryForTesting(nullptr);
  }

  // Set the v1 device manager's synced devices to correspond to the first
  // |num_devices| of GetV1RemoteDevices().
  void SetV1ManagerDevices(size_t num_devices) {
    ASSERT_TRUE(features::ShouldUseV1DeviceSync());

    static const base::NoDestructor<std::vector<cryptauth::ExternalDeviceInfo>>
        device_info([] {
          std::vector<cryptauth::ExternalDeviceInfo> device_info;
          for (const auto& remote_device : GetV1RemoteDevices()) {
            // Add an cryptauth::ExternalDeviceInfo with the same public key as
            // the multidevice::RemoteDevice.
            cryptauth::ExternalDeviceInfo info;
            info.set_public_key(remote_device.public_key);
            device_info.push_back(info);
          }
          return device_info;
        }());

    if (num_devices == 0) {
      fake_device_manager_->synced_devices().clear();
      return;
    }

    DCHECK_LE(num_devices, device_info->size());
    fake_device_manager_->set_synced_devices(
        std::vector<cryptauth::ExternalDeviceInfo>(
            device_info->cbegin(), device_info->cbegin() + num_devices));
  }

  // Set the v2 device manager's synced devices to correspond to the first
  // |num_devices| of GetV2RemoteDevices().
  void SetV2ManagerDevices(size_t num_devices) {
    ASSERT_TRUE(features::ShouldUseV2DeviceSync());

    DCHECK_EQ(5u, GetV2RemoteDevices().size());
    static const CryptAuthDeviceRegistry::InstanceIdToDeviceMap
        cryptauth_devices{
            {GetV2RemoteDevices()[0].instance_id,
             ConvertRemoteDeviceToCryptAuthDevice(GetV2RemoteDevices()[0])},
            {GetV2RemoteDevices()[1].instance_id,
             ConvertRemoteDeviceToCryptAuthDevice(GetV2RemoteDevices()[1])},
            {GetV2RemoteDevices()[2].instance_id,
             ConvertRemoteDeviceToCryptAuthDevice(GetV2RemoteDevices()[2])},
            {GetV2RemoteDevices()[3].instance_id,
             ConvertRemoteDeviceToCryptAuthDevice(GetV2RemoteDevices()[3])},
            {GetV2RemoteDevices()[4].instance_id,
             ConvertRemoteDeviceToCryptAuthDevice(GetV2RemoteDevices()[4])},
        };

    if (num_devices == 0) {
      fake_v2_device_manager_->synced_devices().clear();
      return;
    }

    DCHECK_LE(num_devices, cryptauth_devices.size());
    fake_v2_device_manager_->synced_devices() =
        CryptAuthDeviceRegistry::InstanceIdToDeviceMap(
            cryptauth_devices.cbegin(),
            cryptauth_devices.cbegin() + num_devices);
  }

  void CreateRemoteDeviceProvider() {
    remote_device_provider_ = std::make_unique<RemoteDeviceProviderImpl>(
        fake_v2_device_manager_.get(), kTestUserEmail, kTestUserPrivateKey);
    remote_device_provider_->AddObserver(test_observer_.get());
    EXPECT_EQ(0u, remote_device_provider_->GetSyncedDevices().size());

    // A new loader should be created to load the initial list of devices.
    if (features::ShouldUseV2DeviceSync()) {
      ++expected_v2_loader_count_;
      EXPECT_EQ(expected_v2_loader_count_,
                fake_remote_device_v2_loader_factory_->instances().size());
    }
  }

  void NotifyV1SyncFinished(bool success, bool did_devices_change) {
    ASSERT_TRUE(features::ShouldUseV1DeviceSync());

    fake_device_manager_->NotifySyncFinished(
        success ? CryptAuthDeviceManager::SyncResult::SUCCESS
                : CryptAuthDeviceManager::SyncResult::FAILURE,
        did_devices_change
            ? CryptAuthDeviceManager::DeviceChangeResult::CHANGED
            : CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED);
  }

  void NotifyV2SyncFinished(bool success, bool did_devices_change) {
    ASSERT_TRUE(features::ShouldUseV2DeviceSync());

    fake_v2_device_manager_->NotifyDeviceSyncFinished(CryptAuthDeviceSyncResult(
        success ? CryptAuthDeviceSyncResult::ResultCode::kSuccess
                : CryptAuthDeviceSyncResult::ResultCode::
                      kErrorSyncMetadataApiCallBadRequest,
        did_devices_change, std::nullopt /* client_directive */));

    // A new loader should be created after a successful v2 DeviceSync that
    // changed the device registry.
    if (success && did_devices_change)
      ++expected_v2_loader_count_;

    EXPECT_EQ(expected_v2_loader_count_,
              fake_remote_device_v2_loader_factory_->instances().size());
  }

  void RunV1RemoteDeviceLoader() {
    ASSERT_TRUE(features::ShouldUseV1DeviceSync());
    ASSERT_TRUE(test_device_loader_factory_->HasQueuedCallback());
    test_device_loader_factory_->InvokeLastCallback(
        fake_device_manager_->GetSyncedDevices());
  }

  void RunV2RemoteDeviceLoader() {
    ASSERT_TRUE(features::ShouldUseV2DeviceSync());

    FakeRemoteDeviceV2Loader* loader =
        fake_remote_device_v2_loader_factory_->instances().back();
    EXPECT_TRUE(loader->id_to_device_map());
    EXPECT_EQ(fake_v2_device_manager_->GetSyncedDevices(),
              *loader->id_to_device_map());
    EXPECT_TRUE(loader->user_email());
    EXPECT_EQ(kTestUserEmail, *loader->user_email());
    EXPECT_TRUE(loader->user_private_key());
    EXPECT_EQ(kTestUserPrivateKey, *loader->user_private_key());

    multidevice::RemoteDeviceList loaded_remote_devices;
    for (const multidevice::RemoteDevice& remote_device :
         GetV2RemoteDevices()) {
      if (base::Contains(*loader->id_to_device_map(),
                         remote_device.instance_id)) {
        loaded_remote_devices.push_back(remote_device);
      }
    }
    std::move(loader->callback()).Run(loaded_remote_devices);
  }

  // Verifies the output of the RemoteDeviceProvider.
  void VerifySyncedDevices(
      const multidevice::RemoteDeviceList& expected_remote_devices) {
    EXPECT_EQ(expected_remote_devices.size(),
              remote_device_provider_->GetSyncedDevices().size());
    EXPECT_EQ(
        base::flat_set<multidevice::RemoteDevice>(expected_remote_devices),
        base::flat_set<multidevice::RemoteDevice>(
            remote_device_provider_->GetSyncedDevices()));
  }

  // Verifies that the output of the RemoteDeviceProvider corresponds to the
  // first |expected_num_devices| of GetV1RemoteDevices().
  void VerifyV1SyncedDevices(size_t expected_num_devices) {
    VerifySyncedDevices(multidevice::RemoteDeviceList(
        GetV1RemoteDevices().cbegin(),
        GetV1RemoteDevices().cbegin() + expected_num_devices));
  }

  // Verifies that the output of the RemoteDeviceProvider corresponds to the
  // first |expected_num_devices| of GetV2RemoteDevices().
  void VerifyV2SyncedDevices(size_t expected_num_devices) {
    VerifySyncedDevices(multidevice::RemoteDeviceList(
        GetV2RemoteDevices().cbegin(),
        GetV2RemoteDevices().cbegin() + expected_num_devices));
  }

  std::unique_ptr<FakeDeviceLoader::TestRemoteDeviceLoaderFactory>
      test_device_loader_factory_;
  std::unique_ptr<TestObserver> test_observer_;

 private:
  size_t expected_v2_loader_count_ = 0;
  std::unique_ptr<multidevice::FakeSecureMessageDelegateFactory>
      fake_secure_message_delegate_factory_;
  std::unique_ptr<FakeCryptAuthDeviceManager> fake_device_manager_;
  std::unique_ptr<FakeCryptAuthV2DeviceManager> fake_v2_device_manager_;
  std::unique_ptr<FakeRemoteDeviceV2LoaderFactory>
      fake_remote_device_v2_loader_factory_;
  std::unique_ptr<RemoteDeviceProviderImpl> remote_device_provider_;
};

// ---------------------------------- V2 Only ----------------------------------

TEST_F(DeviceSyncRemoteDeviceProviderImplTest, TestMultipleSyncs_V2Only) {
  // Initialize with devices 0 and 1.
  SetV2ManagerDevices(2u /* num_devices */);
  CreateRemoteDeviceProvider();
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(2u /* expected_num_devices */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  // Now add device 2 and trigger another sync.
  SetV2ManagerDevices(3u /* num_devices */);
  NotifyV2SyncFinished(true /* success */, true /* did_devices_change */);
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(3u /* expected_num_devices */);
  EXPECT_EQ(2, test_observer_->num_times_device_list_changed());

  // Now, simulate a sync which shows that device 2 was removed.
  SetV2ManagerDevices(2u /* num_devices */);
  NotifyV2SyncFinished(true /* success */, true /* did_devices_change */);
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(2u /* expected_num_devices */);
  EXPECT_EQ(3, test_observer_->num_times_device_list_changed());
}

TEST_F(DeviceSyncRemoteDeviceProviderImplTest,
       TestNotifySyncFinishedParameterCombinations_V2Only) {
  SetV2ManagerDevices(1u /* num_devices */);
  CreateRemoteDeviceProvider();
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(1u /* expected_num_devices */);

  NotifyV2SyncFinished(false /* success */, true /* did_devices_change */);
  VerifyV2SyncedDevices(1u /* expected_num_devices */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  NotifyV2SyncFinished(true /* success */, false /* did_devices_change */);
  VerifyV2SyncedDevices(1u /* expected_num_devices */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  NotifyV2SyncFinished(false /* success */, false /* did_devices_change */);
  VerifyV2SyncedDevices(1u /* expected_num_devices */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  SetV2ManagerDevices(2u /* num_devices */);
  NotifyV2SyncFinished(true /* success */, true /* did_devices_change */);
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(2u /* expected_num_devices */);
  EXPECT_EQ(2, test_observer_->num_times_device_list_changed());
}

TEST_F(DeviceSyncRemoteDeviceProviderImplTest,
       TestNewSyncDuringDeviceRegeneration_V2Only) {
  SetV2ManagerDevices(1u /* num_devices */);
  CreateRemoteDeviceProvider();
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(1u /* expected_num_devices */);

  // Add device 1 and trigger a sync.
  SetV2ManagerDevices(2u /* num_devices */);
  NotifyV2SyncFinished(true /* success */, true /* did_devices_change */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  // Do not wait for the new devices to be generated (i.e., don't call
  // RunV2RemoteDeviceLoader() yet). Trigger a new sync with device 2 included.
  SetV2ManagerDevices(3u /* num_devices */);
  NotifyV2SyncFinished(true /* success */, true /* did_devices_change */);
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(3u /* expected_num_devices */);
  EXPECT_EQ(2, test_observer_->num_times_device_list_changed());
}

TEST_F(DeviceSyncRemoteDeviceProviderImplTest, TestZeroSyncedDevices_V2Only) {
  CreateRemoteDeviceProvider();
  RunV2RemoteDeviceLoader();
  VerifyV2SyncedDevices(0u /* expected_num_devices */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());
  NotifyV2SyncFinished(true /* success */, false /* did_devices_change */);
  VerifyV2SyncedDevices(0u /* expected_num_devices */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());
}

}  // namespace device_sync

}  // namespace ash
