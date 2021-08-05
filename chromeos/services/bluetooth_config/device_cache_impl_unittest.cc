// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_cache_impl.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

using PairedDeviceList = std::vector<mojom::PairedBluetoothDevicePropertiesPtr>;
using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";

class FakeObserver : public DeviceCache::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

 private:
  // DeviceCache::Observer:
  void OnPairedDevicesListChanged() override { ++num_calls_; }

  size_t num_calls_ = 0u;
};

}  // namespace

class DeviceCacheImplTest : public testing::Test {
 protected:
  DeviceCacheImplTest() = default;
  DeviceCacheImplTest(const DeviceCacheImplTest&) = delete;
  DeviceCacheImplTest& operator=(const DeviceCacheImplTest&) = delete;
  ~DeviceCacheImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetDevices())
        .WillByDefault(
            testing::Invoke(this, &DeviceCacheImplTest::GenerateDevices));
  }

  void TearDown() override { device_cache_->RemoveObserver(&fake_observer_); }

  void Init() {
    device_cache_ = std::make_unique<DeviceCacheImpl>(
        &fake_adapter_state_controller_, mock_adapter_);
    device_cache_->AddObserver(&fake_observer_);
  }

  void AddDevice(bool paired, bool connected, std::string* id_out) {
    // We use the number of devices created in this test as the address.
    std::string address = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    // Mock devices have their ID set to "${address}-Identifier".
    *id_out = base::StrCat({address, "-Identifier"});

    auto mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), kTestBluetoothClass, kTestBluetoothName,
            address, paired, connected);
    device::BluetoothDevice* device = mock_device.get();
    mock_devices_.push_back(std::move(mock_device));

    if (device_cache_)
      device_cache_->DeviceAdded(mock_adapter_.get(), device);
  }

  void RemoveDevice(const std::string& device_id) {
    auto it = FindDevice(device_id);
    EXPECT_TRUE(it != mock_devices_.end());

    NiceMockDevice device = std::move(*it);
    mock_devices_.erase(it);

    device_cache_->DeviceRemoved(mock_adapter_.get(), device.get());
  }

  void ChangePairingState(const std::string& device_id, bool is_now_paired) {
    auto it = FindDevice(device_id);
    EXPECT_TRUE(it != mock_devices_.end());

    ON_CALL(**it, IsPaired()).WillByDefault(testing::Return(is_now_paired));

    device_cache_->DevicePairedChanged(mock_adapter_.get(), it->get(),
                                       is_now_paired);
  }

  void ChangeDeviceType(const std::string& device_id,
                        device::BluetoothDeviceType new_type) {
    auto it = FindDevice(device_id);
    EXPECT_TRUE(it != mock_devices_.end());

    ON_CALL(**it, GetDeviceType()).WillByDefault(testing::Return(new_type));

    device_cache_->DeviceChanged(mock_adapter_.get(), it->get());
  }

  PairedDeviceList GetPairedDevices() {
    return device_cache_->GetPairedDevices();
  }

  size_t GetNumObserverEvents() const { return fake_observer_.num_calls(); }

 private:
  std::vector<const device::BluetoothDevice*> GenerateDevices() {
    std::vector<const device::BluetoothDevice*> devices;
    for (auto& device : mock_devices_)
      devices.push_back(device.get());
    return devices;
  }

  std::vector<NiceMockDevice>::iterator FindDevice(
      const std::string& device_id) {
    return std::find_if(mock_devices_.begin(), mock_devices_.end(),
                        [&device_id](const NiceMockDevice& device) {
                          return device_id == device->GetIdentifier();
                        });
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  FakeAdapterStateController fake_adapter_state_controller_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  FakeObserver fake_observer_;

  std::unique_ptr<DeviceCacheImpl> device_cache_;
};

TEST_F(DeviceCacheImplTest, AddAndRemovePairedDevices) {
  Init();
  EXPECT_TRUE(GetPairedDevices().empty());

  // Add device 1 (disconnected).
  std::string paired_device_id1;
  AddDevice(/*paired=*/true, /*connected=*/false, &paired_device_id1);
  EXPECT_EQ(1u, GetNumObserverEvents());
  PairedDeviceList list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id1, list[0]->device_properties->id);

  // Add device 2 (connected). Paired connected devices should be returned
  // before disconnected ones.
  std::string paired_device_id2;
  AddDevice(/*paired=*/true, /*connected=*/true, &paired_device_id2);
  EXPECT_EQ(2u, GetNumObserverEvents());
  list = GetPairedDevices();
  EXPECT_EQ(2u, list.size());
  EXPECT_EQ(paired_device_id2, list[0]->device_properties->id);
  EXPECT_EQ(paired_device_id1, list[1]->device_properties->id);

  // Remove device 2; only device 1 should be returned.
  RemoveDevice(paired_device_id2);
  EXPECT_EQ(3u, GetNumObserverEvents());
  list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id1, list[0]->device_properties->id);
}

TEST_F(DeviceCacheImplTest, AddBeforeInit) {
  // Add device 1 before initializing the class.
  std::string paired_device_id1;
  AddDevice(/*paired=*/true, /*connected=*/true, &paired_device_id1);
  Init();

  // Device 1 should be available from the getgo.
  PairedDeviceList list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id1, list[0]->device_properties->id);

  // Add device 2 and verify that both devices are returned.
  std::string paired_device_id2;
  AddDevice(/*paired=*/true, /*connected=*/true, &paired_device_id2);
  EXPECT_EQ(1u, GetNumObserverEvents());
  list = GetPairedDevices();
  EXPECT_EQ(2u, list.size());
  EXPECT_EQ(paired_device_id1, list[0]->device_properties->id);
  EXPECT_EQ(paired_device_id2, list[1]->device_properties->id);
}

TEST_F(DeviceCacheImplTest, UnpairedDeviceNotReturned) {
  Init();
  EXPECT_TRUE(GetPairedDevices().empty());

  // Add a paired device.
  std::string paired_device_id;
  AddDevice(/*paired=*/true, /*connected=*/true, &paired_device_id);
  EXPECT_EQ(1u, GetNumObserverEvents());
  PairedDeviceList list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id, list[0]->device_properties->id);

  // Add an unpaired device; observers should not be notified, and the unpaired
  // device should not be returned.
  std::string unpaired_device_id;
  AddDevice(/*paired=*/false, /*connected=*/true, &unpaired_device_id);
  EXPECT_EQ(1u, GetNumObserverEvents());
  list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id, list[0]->device_properties->id);
}

TEST_F(DeviceCacheImplTest, PairingStateChanges) {
  Init();
  EXPECT_TRUE(GetPairedDevices().empty());

  // Add a paired device.
  std::string paired_device_id;
  AddDevice(/*paired=*/true, /*connected=*/true, &paired_device_id);
  EXPECT_EQ(1u, GetNumObserverEvents());
  PairedDeviceList list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id, list[0]->device_properties->id);

  // Unpair; the observer should be notified, and the device should not be
  // returned.
  ChangePairingState(paired_device_id, /*is_now_paired=*/false);
  EXPECT_EQ(2u, GetNumObserverEvents());
  EXPECT_TRUE(GetPairedDevices().empty());

  // Re-pair; the observer should be notified, and the device should be
  // returned.
  ChangePairingState(paired_device_id, /*is_now_paired=*/true);
  EXPECT_EQ(3u, GetNumObserverEvents());
  list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id, list[0]->device_properties->id);
}

TEST_F(DeviceCacheImplTest, BluetoothClassChanges) {
  Init();
  EXPECT_TRUE(GetPairedDevices().empty());

  // Add a paired device.
  std::string paired_device_id;
  AddDevice(/*paired=*/true, /*connected=*/true, &paired_device_id);
  EXPECT_EQ(1u, GetNumObserverEvents());
  PairedDeviceList list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id, list[0]->device_properties->id);
  EXPECT_EQ(mojom::DeviceType::kUnknown,
            list[0]->device_properties->device_type);

  // Change its device type.
  ChangeDeviceType(paired_device_id, device::BluetoothDeviceType::PHONE);
  EXPECT_EQ(3u, GetNumObserverEvents());
  list = GetPairedDevices();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(paired_device_id, list[0]->device_properties->id);
  EXPECT_EQ(mojom::DeviceType::kPhone, list[0]->device_properties->device_type);
}

}  // namespace bluetooth_config
}  // namespace chromeos
