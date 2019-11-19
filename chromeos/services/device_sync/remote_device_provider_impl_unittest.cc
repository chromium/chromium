// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/remote_device_provider_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chromeos/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/services/device_sync/fake_cryptauth_device_manager.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/remote_device_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace device_sync {

namespace {

const char kTestUserId[] = "testUserId";
const char kTestUserPrivateKey[] = "kTestUserPrivateKey";

std::vector<cryptauth::ExternalDeviceInfo>
CreateExternalDeviceInfosForRemoteDevices(
    const multidevice::RemoteDeviceList remote_devices) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos;
  for (const auto& remote_device : remote_devices) {
    // Add an cryptauth::ExternalDeviceInfo with the same public key as the
    // multidevice::RemoteDevice.
    cryptauth::ExternalDeviceInfo info;
    info.set_public_key(remote_device.public_key);
    device_infos.push_back(info);
  }
  return device_infos;
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
    TestRemoteDeviceLoaderFactory()
        : test_devices_(multidevice::CreateRemoteDeviceListForTest(5)),
          test_device_infos_(
              CreateExternalDeviceInfosForRemoteDevices(test_devices_)) {}

    ~TestRemoteDeviceLoaderFactory() {}

    std::unique_ptr<RemoteDeviceLoader> BuildInstance(
        const std::vector<cryptauth::ExternalDeviceInfo>& device_info_list,
        const std::string& user_id,
        const std::string& user_private_key,
        std::unique_ptr<multidevice::SecureMessageDelegate>
            secure_message_delegate) override {
      EXPECT_EQ(std::string(kTestUserId), user_id);
      EXPECT_EQ(std::string(kTestUserPrivateKey), user_private_key);
      std::unique_ptr<FakeDeviceLoader> device_loader =
          std::make_unique<FakeDeviceLoader>();
      device_loader->remote_device_loader_factory_ = this;
      return std::move(device_loader);
    }

    void InvokeLastCallback(
        const std::vector<cryptauth::ExternalDeviceInfo>& device_info_list) {
      ASSERT_TRUE(!callback_.is_null());
      // Fetch only the devices inserted by tests, since test_devices_ contains
      // all available devices.
      multidevice::RemoteDeviceList devices;
      for (const auto remote_device : test_devices_) {
        for (const auto& external_device_info : device_info_list) {
          if (remote_device.public_key == external_device_info.public_key())
            devices.push_back(remote_device);
        }
      }
      callback_.Run(devices);
      callback_.Reset();
    }

    // Fetch is only started if the change result passed to OnSyncFinished() is
    // CHANGED and sync is SUCCESS.
    bool HasQueuedCallback() { return !callback_.is_null(); }
    const multidevice::RemoteDeviceList test_devices_;
    const std::vector<cryptauth::ExternalDeviceInfo> test_device_infos_;

    void QueueCallback(const RemoteDeviceCallback& callback) {
      callback_ = callback;
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

  TestRemoteDeviceLoaderFactory* remote_device_loader_factory_;

  void Load(const RemoteDeviceCallback& callback) override {
    remote_device_loader_factory_->QueueCallback(callback);
  }
};

class DeviceSyncRemoteDeviceProviderImplTest : public testing::Test {
 public:
  DeviceSyncRemoteDeviceProviderImplTest() {}

  void SetUp() override {
    fake_device_manager_ = std::make_unique<FakeCryptAuthDeviceManager>();
    fake_secure_message_delegate_factory_ =
        std::make_unique<multidevice::FakeSecureMessageDelegateFactory>();
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        fake_secure_message_delegate_factory_.get());
    test_device_loader_factory_ =
        std::make_unique<FakeDeviceLoader::TestRemoteDeviceLoaderFactory>();
    RemoteDeviceLoader::Factory::SetInstanceForTesting(
        test_device_loader_factory_.get());
    test_observer_ = std::make_unique<TestObserver>();
  }

  void TearDown() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        nullptr);
  }

  void CreateRemoteDeviceProvider() {
    remote_device_provider_ = std::make_unique<RemoteDeviceProviderImpl>(
        fake_device_manager_.get(), kTestUserId, kTestUserPrivateKey);
    remote_device_provider_->AddObserver(test_observer_.get());
    EXPECT_EQ(0u, remote_device_provider_->GetSyncedDevices().size());
    test_device_loader_factory_->InvokeLastCallback(
        fake_device_manager_->GetSyncedDevices());
    VerifySyncedDevicesMatchExpectation(
        fake_device_manager_->GetSyncedDevices().size());
  }

  void VerifySyncedDevicesMatchExpectation(size_t expected_size) {
    multidevice::RemoteDeviceList synced_devices =
        remote_device_provider_->GetSyncedDevices();
    EXPECT_EQ(expected_size, synced_devices.size());
    EXPECT_EQ(expected_size, fake_device_manager_->GetSyncedDevices().size());
    std::unordered_set<std::string> public_keys;
    for (const auto& device_info : fake_device_manager_->GetSyncedDevices()) {
      public_keys.insert(device_info.public_key());
    }
    for (const auto& remote_device : synced_devices) {
      EXPECT_TRUE(public_keys.find(remote_device.public_key) !=
                  public_keys.end());
    }
  }

  multidevice::RemoteDeviceList test_devices() {
    return test_device_loader_factory_->test_devices_;
  }

  cryptauth::ExternalDeviceInfo test_device_infos_(int val) {
    return test_device_loader_factory_->test_device_infos_[val];
  }

  std::unique_ptr<multidevice::FakeSecureMessageDelegateFactory>
      fake_secure_message_delegate_factory_;

  std::unique_ptr<FakeCryptAuthDeviceManager> fake_device_manager_;

  std::unique_ptr<FakeDeviceLoader::TestRemoteDeviceLoaderFactory>
      test_device_loader_factory_;

  std::unique_ptr<RemoteDeviceProviderImpl> remote_device_provider_;

  std::unique_ptr<TestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSyncRemoteDeviceProviderImplTest);
};

TEST_F(DeviceSyncRemoteDeviceProviderImplTest, TestMultipleSyncs) {
  // Initialize with devices 0 and 1.
  fake_device_manager_->synced_devices().push_back(test_device_infos_(0));
  fake_device_manager_->synced_devices().push_back(test_device_infos_(1));
  CreateRemoteDeviceProvider();
  VerifySyncedDevicesMatchExpectation(2u /* expected_size */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  // Now add device 2 and trigger another sync.
  fake_device_manager_->synced_devices().push_back(test_device_infos_(2));
  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::SUCCESS,
      CryptAuthDeviceManager::DeviceChangeResult::CHANGED);
  test_device_loader_factory_->InvokeLastCallback(
      fake_device_manager_->GetSyncedDevices());
  VerifySyncedDevicesMatchExpectation(3u /* expected_size */);
  EXPECT_EQ(2, test_observer_->num_times_device_list_changed());

  // Now, simulate a sync which shows that device 0 was removed.
  fake_device_manager_->synced_devices().erase(
      fake_device_manager_->synced_devices().begin());
  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::SUCCESS,
      CryptAuthDeviceManager::DeviceChangeResult::CHANGED);
  test_device_loader_factory_->InvokeLastCallback(
      fake_device_manager_->GetSyncedDevices());
  VerifySyncedDevicesMatchExpectation(2u /* expected_size */);
  EXPECT_EQ(3, test_observer_->num_times_device_list_changed());
}

TEST_F(DeviceSyncRemoteDeviceProviderImplTest,
       TestNotifySyncFinishedParameterCombinations) {
  fake_device_manager_->synced_devices().push_back(test_device_infos_(0));
  CreateRemoteDeviceProvider();
  VerifySyncedDevicesMatchExpectation(1u /* expected_size */);
  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::FAILURE,
      CryptAuthDeviceManager::DeviceChangeResult::CHANGED);
  EXPECT_FALSE(test_device_loader_factory_->HasQueuedCallback());
  VerifySyncedDevicesMatchExpectation(1u /* expected_size */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::SUCCESS,
      CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED);
  EXPECT_FALSE(test_device_loader_factory_->HasQueuedCallback());
  VerifySyncedDevicesMatchExpectation(1u /* expected_size */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::FAILURE,
      CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED);
  EXPECT_FALSE(test_device_loader_factory_->HasQueuedCallback());
  VerifySyncedDevicesMatchExpectation(1u /* expected_size */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  fake_device_manager_->synced_devices().push_back(test_device_infos_(1));
  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::SUCCESS,
      CryptAuthDeviceManager::DeviceChangeResult::CHANGED);
  test_device_loader_factory_->InvokeLastCallback(
      fake_device_manager_->GetSyncedDevices());
  VerifySyncedDevicesMatchExpectation(2u /* expected_size */);
  EXPECT_EQ(2, test_observer_->num_times_device_list_changed());
}

TEST_F(DeviceSyncRemoteDeviceProviderImplTest,
       TestNewSyncDuringDeviceRegeneration) {
  fake_device_manager_->synced_devices().push_back(test_device_infos_(0));
  CreateRemoteDeviceProvider();
  VerifySyncedDevicesMatchExpectation(1u /* expected_size */);

  // Add device 1 and trigger a sync.
  fake_device_manager_->synced_devices().push_back(test_device_infos_(1));
  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::SUCCESS,
      CryptAuthDeviceManager::DeviceChangeResult::CHANGED);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());

  // Do not wait for the new devices to be generated (i.e., don't call
  // test_device_loader_factory_->InvokeLastCallback() yet). Trigger a new
  // sync with device 2 included.
  fake_device_manager_->synced_devices().push_back(test_device_infos_(2));
  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::SUCCESS,
      CryptAuthDeviceManager::DeviceChangeResult::CHANGED);
  test_device_loader_factory_->InvokeLastCallback(
      fake_device_manager_->GetSyncedDevices());
  VerifySyncedDevicesMatchExpectation(3u /* expected_size */);
  EXPECT_EQ(2, test_observer_->num_times_device_list_changed());
}

TEST_F(DeviceSyncRemoteDeviceProviderImplTest, TestZeroSyncedDevices) {
  CreateRemoteDeviceProvider();
  VerifySyncedDevicesMatchExpectation(0 /* expected_size */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());
  fake_device_manager_->NotifySyncFinished(
      CryptAuthDeviceManager::SyncResult::SUCCESS,
      CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED);
  EXPECT_FALSE(test_device_loader_factory_->HasQueuedCallback());
  VerifySyncedDevicesMatchExpectation(0 /* expected_size */);
  EXPECT_EQ(1, test_observer_->num_times_device_list_changed());
}

}  // namespace device_sync

}  // namespace chromeos
