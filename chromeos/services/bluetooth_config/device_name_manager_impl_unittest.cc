// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_name_manager_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";
const char kTestNickname[] = "nickname";

class FakeObserver : public DeviceNameManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_device_nickname_changed_calls() const {
    return num_device_nickname_changed_calls_;
  }

  const std::string& last_device_id_nickname_changed() const {
    return last_device_id_nickname_changed_;
  }

  const std::string& last_device_nickname_changed() const {
    return last_device_nickname_changed_;
  }

 private:
  // DeviceNameManager::Observer:
  void OnDeviceNicknameChanged(const std::string& device_id,
                               const std::string& nickname) override {
    ++num_device_nickname_changed_calls_;
    last_device_id_nickname_changed_ = device_id;
    last_device_nickname_changed_ = nickname;
  }

  size_t num_device_nickname_changed_calls_ = 0u;
  std::string last_device_id_nickname_changed_;
  std::string last_device_nickname_changed_;
};

}  // namespace

class DeviceNameManagerImplTest : public testing::Test {
 protected:
  DeviceNameManagerImplTest() = default;
  DeviceNameManagerImplTest(const DeviceNameManagerImplTest&) = delete;
  DeviceNameManagerImplTest& operator=(const DeviceNameManagerImplTest&) =
      delete;
  ~DeviceNameManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    DeviceNameManagerImpl::RegisterLocalStatePrefs(
        test_pref_service_.registry());

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetDevices())
        .WillByDefault(
            testing::Invoke(this, &DeviceNameManagerImplTest::GetMockDevices));
  }

  std::unique_ptr<DeviceNameManagerImpl> CreateDeviceNameManager() {
    auto device_name_manager =
        std::make_unique<DeviceNameManagerImpl>(mock_adapter_);
    device_name_manager->AddObserver(&fake_observer_);
    device_name_manager->SetPrefs(&test_pref_service_);
    return device_name_manager;
  }

  void AddDevice(std::string* id_out) {
    // We use the number of devices created in this test as the address.
    std::string address = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    // Mock devices have their ID set to "${address}-Identifier".
    *id_out = base::StrCat({address, "-Identifier"});

    auto mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), kTestBluetoothClass, kTestBluetoothName,
            address, /*paired=*/false, /*connected=*/false);
    mock_devices_.push_back(std::move(mock_device));
  }

  size_t GetNumDeviceNicknameObserverEvents() const {
    return fake_observer_.num_device_nickname_changed_calls();
  }

  const std::string& GetLastDeviceIdNicknameChanged() const {
    return fake_observer_.last_device_id_nickname_changed();
  }

  const std::string& GetLastDeviceNicknameChanged() const {
    return fake_observer_.last_device_nickname_changed();
  }

 private:
  std::vector<const device::BluetoothDevice*> GetMockDevices() {
    std::vector<const device::BluetoothDevice*> devices;
    for (auto& device : mock_devices_)
      devices.push_back(device.get());
    return devices;
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  FakeObserver fake_observer_;
};

TEST_F(DeviceNameManagerImplTest, GetThenSetValidThenSetInvalid) {
  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);

  // Set an empty nickname, this should fail and the nickname should be
  // unchanged.
  manager->SetDeviceNickname(device_id, "");
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);

  // Set nickname above character limit, this should also fail and the nickname
  // should be unchanged.
  manager->SetDeviceNickname(device_id, "123456789012345678901234567890123");
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
}

TEST_F(DeviceNameManagerImplTest, NicknameIsPersistedBetweenManagerInstances) {
  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);

  // Create a new manager and destroy the old one.
  manager = CreateDeviceNameManager();
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
}

TEST_F(DeviceNameManagerImplTest, SetNicknameDeviceNotFound) {
  const std::string device_id = "device_id";
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  // Setting the nickname of an unknown device should fail.
  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 0u);
}

}  // namespace bluetooth_config
}  // namespace chromeos
