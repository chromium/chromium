// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/discovered_devices_provider_impl.h"

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/bluetooth_config/device_conversion_util.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

using DiscoveredDevicesList = std::vector<mojom::BluetoothDevicePropertiesPtr>;
using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";

class FakeObserver : public DiscoveredDevicesProvider::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_discovered_devices_list_changed_calls() const {
    return num_discovered_devices_list_changed_calls_;
  }

 private:
  // DiscoveredDevicesProvider::Observer:
  void OnDiscoveredDevicesListChanged() override {
    ++num_discovered_devices_list_changed_calls_;
  }

  size_t num_discovered_devices_list_changed_calls_ = 0u;
};

}  // namespace

class DiscoveredDevicesProviderImplTest : public testing::Test {
 protected:
  DiscoveredDevicesProviderImplTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DiscoveredDevicesProviderImplTest(const DiscoveredDevicesProviderImplTest&) =
      delete;
  DiscoveredDevicesProviderImplTest& operator=(
      const DiscoveredDevicesProviderImplTest&) = delete;
  ~DiscoveredDevicesProviderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    discovered_devices_provider_ =
        std::make_unique<DiscoveredDevicesProviderImpl>(&fake_device_cache_);
    discovered_devices_provider_->AddObserver(&fake_observer_);
  }

  void InsertDevice(int index, std::string* id_out) {
    // We use the number of devices created in this test as the address.
    std::string address = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    // Mock devices have their ID set to "${address}-Identifier".
    *id_out = base::StrCat({address, "-Identifier"});

    auto mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            /*adapter=*/nullptr, kTestBluetoothClass, kTestBluetoothName,
            address, /*paired=*/false, /*connected=*/false);
    ON_CALL(*mock_device, GetDeviceType())
        .WillByDefault(testing::Return(device::BluetoothDeviceType::AUDIO));
    mock_devices_.insert(mock_devices_.begin() + index, std::move(mock_device));

    UpdateDeviceCache();
  }

  void UpdateDevicePosition(const std::string& device_id, int new_index) {
    auto it = FindDevice(device_id);
    EXPECT_TRUE(it != mock_devices_.end());
    NiceMockDevice device = std::move(*it);

    mock_devices_.erase(it);
    mock_devices_.insert(mock_devices_.begin() + new_index, std::move(device));

    UpdateDeviceCache();
  }

  void UpdateDeviceType(const std::string& device_id,
                        device::BluetoothDeviceType new_type) {
    auto it = FindDevice(device_id);
    EXPECT_TRUE(it != mock_devices_.end());
    ON_CALL(**it, GetDeviceType()).WillByDefault(testing::Return(new_type));

    UpdateDeviceCache();
  }

  void RemoveDevice(const std::string& device_id) {
    auto it = FindDevice(device_id);
    EXPECT_TRUE(it != mock_devices_.end());
    mock_devices_.erase(it);

    UpdateDeviceCache();
  }

  int64_t GetNotificationDelaySeconds() {
    return DiscoveredDevicesProviderImpl::kNotificationDelay.InSeconds();
  }

  void FastForward(int64_t seconds) {
    task_environment_.FastForwardBy(base::Seconds(seconds));
  }

  DiscoveredDevicesList GetDiscoveredDevices() {
    return discovered_devices_provider_->GetDiscoveredDevices();
  }

  size_t GetNumDiscoveredDevicesListObserverEvents() const {
    return fake_observer_.num_discovered_devices_list_changed_calls();
  }

  void AssertNumEventsAndDeviceList(size_t num_observer_events,
                                    std::vector<std::string> device_list) {
    EXPECT_EQ(num_observer_events, GetNumDiscoveredDevicesListObserverEvents());
    DiscoveredDevicesList discovered_devices = GetDiscoveredDevices();
    EXPECT_EQ(device_list.size(), discovered_devices.size());
    for (size_t i = 0; i < device_list.size(); i++) {
      EXPECT_EQ(device_list[i], discovered_devices[i]->id);
    }
  }

 private:
  std::vector<NiceMockDevice>::iterator FindDevice(
      const std::string& device_id) {
    return base::ranges::find(
        mock_devices_, device_id,
        &testing::NiceMock<device::MockBluetoothDevice>::GetIdentifier);
  }

  // Copies the list of devices in |mock_devices_| to device's caches unpaired
  // devices.
  void UpdateDeviceCache() {
    std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices;
    for (auto& device : mock_devices_) {
      unpaired_devices.push_back(GenerateBluetoothDeviceMojoProperties(
          device.get(), /*fast_pair_delegate=*/nullptr));
    }
    fake_device_cache_.SetUnpairedDevices(std::move(unpaired_devices));
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  FakeAdapterStateController fake_adapter_state_controller_;
  FakeDeviceCache fake_device_cache_{&fake_adapter_state_controller_};
  FakeObserver fake_observer_;

  std::unique_ptr<DiscoveredDevicesProvider> discovered_devices_provider_;
};

TEST_F(DiscoveredDevicesProviderImplTest, AddUnpairedDevices) {
  EXPECT_EQ(0u, GetNumDiscoveredDevicesListObserverEvents());
  EXPECT_TRUE(GetDiscoveredDevices().empty());

  // Add a device. This should notify observers and start the timer.
  std::string device_id1;
  InsertDevice(/*index=*/0, &device_id1);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/1u, {device_id1});

  // Simulate |seconds_forward| seconds passing.
  int seconds_forward = 1;
  FastForward(seconds_forward);

  // Insert a second device at the front of the list. Observers should be
  // notified, but the second device placed at the end of the list.
  std::string device_id2;
  InsertDevice(/*index=*/0, &device_id2);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/2u,
                               {device_id1, device_id2});

  // Simulate the notification delay passing since the first device was added.
  // Observers be notified again with the correctly sorted list.
  FastForward(GetNotificationDelaySeconds() - seconds_forward);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/3u,
                               {device_id2, device_id1});

  // Simulate moving to the time to where the second device's timer would
  // expire, if it was set. Nothing new should have occurred since the timer
  // shouldn't have been set.
  FastForward(/*seconds=*/seconds_forward);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/3u,
                               {device_id2, device_id1});

  // Insert a third device at the front of the list. Observers should be
  // notified, but the third device placed at the end of the list.
  std::string device_id3;
  InsertDevice(/*index=*/0, &device_id3);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/4u,
                               {device_id2, device_id1, device_id3});

  // Simulate the notification delay passing. Observers should be notified again
  // with the correctly sorted list.
  FastForward(GetNotificationDelaySeconds());
  AssertNumEventsAndDeviceList(/*num_observer_events=*/5u,
                               {device_id3, device_id2, device_id1});
}

TEST_F(DiscoveredDevicesProviderImplTest, UpdateUnpairedDevices) {
  EXPECT_EQ(0u, GetNumDiscoveredDevicesListObserverEvents());
  EXPECT_TRUE(GetDiscoveredDevices().empty());

  // Add three devices. This should notify observers and start the timer.
  std::string device_id1;
  InsertDevice(/*index=*/0, &device_id1);
  std::string device_id2;
  InsertDevice(/*index=*/1, &device_id2);
  std::string device_id3;
  InsertDevice(/*index=*/2, &device_id3);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/3u,
                               {device_id1, device_id2, device_id3});

  // Simulate the notification delay passing. Observers should be notified
  // again.
  FastForward(GetNotificationDelaySeconds());
  AssertNumEventsAndDeviceList(/*num_observer_events=*/4u,
                               {device_id1, device_id2, device_id3});

  // Update device 2. This should notify observers and start the timer, with the
  // ordering kept same.
  EXPECT_EQ(mojom::DeviceType::kHeadset,
            GetDiscoveredDevices()[1]->device_type);
  UpdateDeviceType(device_id2, device::BluetoothDeviceType::VIDEO);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/5u,
                               {device_id1, device_id2, device_id3});
  EXPECT_EQ(mojom::DeviceType::kVideoCamera,
            GetDiscoveredDevices()[1]->device_type);

  // Simulate the notification delay passing. Observers should be notified again
  // but nothing changed.
  FastForward(GetNotificationDelaySeconds());
  AssertNumEventsAndDeviceList(/*num_observer_events=*/6u,
                               {device_id1, device_id2, device_id3});
  EXPECT_EQ(mojom::DeviceType::kVideoCamera,
            GetDiscoveredDevices()[1]->device_type);

  // Move device 3 to the front of the list. Observers should be notified but
  // the order should remain the same.
  UpdateDevicePosition(device_id3, /*new_index=*/0);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/7u,
                               {device_id1, device_id2, device_id3});

  // Simulate the notification delay passing. Observers should be notified with
  // the correct ordering.
  FastForward(GetNotificationDelaySeconds());
  AssertNumEventsAndDeviceList(/*num_observer_events=*/8u,
                               {device_id3, device_id1, device_id2});

  // Update device 3. This should notify observers and start the timer, with the
  // ordering kept same.
  EXPECT_EQ(mojom::DeviceType::kHeadset,
            GetDiscoveredDevices()[0]->device_type);
  UpdateDeviceType(device_id3, device::BluetoothDeviceType::VIDEO);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/9u,
                               {device_id3, device_id1, device_id2});
  EXPECT_EQ(mojom::DeviceType::kVideoCamera,
            GetDiscoveredDevices()[0]->device_type);

  // Simulate |seconds_forward| seconds passing.
  int seconds_forward = 1;
  FastForward(seconds_forward);

  // Move device 1 to the front of the list. Observers should be notified but
  // the order should remain the same.
  UpdateDevicePosition(device_id1, /*new_index=*/0);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/10u,
                               {device_id3, device_id1, device_id2});
  EXPECT_EQ(mojom::DeviceType::kVideoCamera,
            GetDiscoveredDevices()[0]->device_type);

  // Simulate the notification delay passing since device 3 was updated.
  // Observers be notified again with the correctly sorted list.
  FastForward(GetNotificationDelaySeconds() - seconds_forward);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/11u,
                               {device_id1, device_id3, device_id2});
  EXPECT_EQ(mojom::DeviceType::kVideoCamera,
            GetDiscoveredDevices()[1]->device_type);

  // Simulate moving to the time to where the device 1 update's timer would
  // expire, if it was set. Nothing new should have occurred since the timer
  // shouldn't have been set.
  FastForward(/*seconds=*/seconds_forward);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/11u,
                               {device_id1, device_id3, device_id2});
  EXPECT_EQ(mojom::DeviceType::kVideoCamera,
            GetDiscoveredDevices()[1]->device_type);
}

TEST_F(DiscoveredDevicesProviderImplTest, RemoveUnpairedDevices) {
  EXPECT_EQ(0u, GetNumDiscoveredDevicesListObserverEvents());
  EXPECT_TRUE(GetDiscoveredDevices().empty());

  // Add three devices. This should notify observers and start the timer.
  std::string device_id1;
  InsertDevice(/*index=*/0, &device_id1);
  std::string device_id2;
  InsertDevice(/*index=*/1, &device_id2);
  std::string device_id3;
  InsertDevice(/*index=*/2, &device_id3);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/3u,
                               {device_id1, device_id2, device_id3});

  // Simulate the notification delay passing. Observers should be notified
  // again.
  FastForward(GetNotificationDelaySeconds());
  AssertNumEventsAndDeviceList(/*num_observer_events=*/4u,
                               {device_id1, device_id2, device_id3});

  // Remove device 2. This should notify observers with the updated list and
  // start the timer.
  RemoveDevice(device_id2);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/5u,
                               {device_id1, device_id3});

  // Simulate the notification delay passing. Observers should be notified
  // again.
  FastForward(GetNotificationDelaySeconds());
  AssertNumEventsAndDeviceList(/*num_observer_events=*/6u,
                               {device_id1, device_id3});

  // Remove device 1. This should notify observers with the updated list and
  // start the timer.
  RemoveDevice(device_id1);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/7u, {device_id3});

  // Simulate |seconds_forward| seconds passing.
  int seconds_forward = 1;
  FastForward(seconds_forward);

  // Remove device 3. This should notify observers with the updated list.
  RemoveDevice(device_id3);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/8u, {});

  // Simulate the notification delay passing since device 1 was removed.
  // Observers be notified again.
  FastForward(GetNotificationDelaySeconds() - seconds_forward);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/9u, {});

  // Simulate moving to the time to where the device 3 removal's timer would
  // expire, if it was set. Nothing new should have occurred since the timer
  // shouldn't have been set.
  FastForward(/*seconds=*/seconds_forward);
  AssertNumEventsAndDeviceList(/*num_observer_events=*/9u, {});
}

}  // namespace ash::bluetooth_config
