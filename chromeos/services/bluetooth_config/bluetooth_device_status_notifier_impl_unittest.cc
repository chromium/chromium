// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/bluetooth_device_status_notifier_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_device_status_observer.h"
#include "chromeos/services/bluetooth_config/fake_device_cache.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

mojom::PairedBluetoothDevicePropertiesPtr GenerateStubPairedDeviceProperties(
    std::string id,
    mojom::DeviceConnectionState connection_state =
        mojom::DeviceConnectionState::kConnected) {
  auto device_properties = mojom::BluetoothDeviceProperties::New();
  device_properties->id = id;
  device_properties->public_name = u"name";
  device_properties->device_type = mojom::DeviceType::kUnknown;
  device_properties->audio_capability =
      mojom::AudioOutputCapability::kNotCapableOfAudioOutput;
  device_properties->connection_state = connection_state;

  mojom::PairedBluetoothDevicePropertiesPtr paired_properties =
      mojom::PairedBluetoothDeviceProperties::New();
  paired_properties->device_properties = std::move(device_properties);
  return paired_properties;
}

}  // namespace

class BluetoothDeviceStatusNotifierImplTest : public testing::Test {
 protected:
  BluetoothDeviceStatusNotifierImplTest() = default;
  BluetoothDeviceStatusNotifierImplTest(
      const BluetoothDeviceStatusNotifierImplTest&) = delete;
  BluetoothDeviceStatusNotifierImplTest& operator=(
      const BluetoothDeviceStatusNotifierImplTest&) = delete;
  ~BluetoothDeviceStatusNotifierImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    device_status_notifier_ =
        std::make_unique<BluetoothDeviceStatusNotifierImpl>(
            &fake_device_cache_);
  }

  void SetPairedDevices(
      const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
          paired_devices) {
    std::vector<mojom::PairedBluetoothDevicePropertiesPtr> copy;
    for (const auto& paired_device : paired_devices)
      copy.push_back(paired_device.Clone());

    fake_device_cache_.SetPairedDevices(std::move(copy));
    device_status_notifier_->FlushForTesting();
  }

  std::unique_ptr<FakeBluetoothDeviceStatusObserver> Observe() {
    auto observer = std::make_unique<FakeBluetoothDeviceStatusObserver>();
    device_status_notifier_->ObserveDeviceStatusChanges(
        observer->GeneratePendingRemote());
    device_status_notifier_->FlushForTesting();
    return observer;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  FakeAdapterStateController fake_adapter_state_controller_;
  FakeDeviceCache fake_device_cache_{&fake_adapter_state_controller_};
  std::unique_ptr<BluetoothDeviceStatusNotifierImpl> device_status_notifier_;
};

TEST_F(BluetoothDeviceStatusNotifierImplTest, PairedDevicesChanges) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();

  // Initially, observer would receive no updates.
  ASSERT_EQ(0u, observer->paired_device_properties_list().size());
  EXPECT_TRUE(observer->paired_device_properties_list().empty());

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices;
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id", /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));

  // Add a paired but disconnected device and verify that the observer was
  // not notified.
  SetPairedDevices(paired_devices);
  ASSERT_EQ(0u, observer->paired_device_properties_list().size());
  EXPECT_TRUE(observer->paired_device_properties_list().empty());

  paired_devices.pop_back();
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id1", /*connection_state=*/mojom::DeviceConnectionState::kConnected));

  // Add a paired device and verify that the observer was notified.
  SetPairedDevices(paired_devices);
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());
  ASSERT_EQ(
      "id1",
      observer->paired_device_properties_list()[0]->device_properties->id);

  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id2", /*connection_state=*/mojom::DeviceConnectionState::kConnected));
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id3", /*connection_state=*/mojom::DeviceConnectionState::kConnected));

  // Add two paired devices and verify that the observer was notified.
  SetPairedDevices(paired_devices);
  ASSERT_EQ(3u, observer->paired_device_properties_list().size());
  ASSERT_EQ(
      "id2",
      observer->paired_device_properties_list()[1]->device_properties->id);
  ASSERT_EQ(
      "id3",
      observer->paired_device_properties_list()[2]->device_properties->id);

  // Simulate device being unpaired, the observer should not be called.
  paired_devices.pop_back();
  SetPairedDevices(paired_devices);
  ASSERT_EQ(3u, observer->paired_device_properties_list().size());

  // Add the same device again.
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id3", /*connection_state=*/mojom::DeviceConnectionState::kConnected));

  // Verify that the observer was notified.
  SetPairedDevices(paired_devices);
  ASSERT_EQ(4u, observer->paired_device_properties_list().size());
  ASSERT_EQ(
      "id3",
      observer->paired_device_properties_list()[3]->device_properties->id);
}

TEST_F(BluetoothDeviceStatusNotifierImplTest, ConnectedDevicesChanges) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> devices;
  devices.push_back(GenerateStubPairedDeviceProperties("id1"));

  // Add a connected device
  SetPairedDevices(devices);

  // Initially, observer would receive no updates.
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  EXPECT_TRUE(observer->disconnected_device_properties_list().empty());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  EXPECT_TRUE(observer->connected_device_properties_list().empty());

  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from connected to disconnected.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      "id1", /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));

  SetPairedDevices(devices);

  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ("id1", observer->disconnected_device_properties_list()[0]
                       ->device_properties->id);
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from disconnected to connected.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties("id1"));
  SetPairedDevices(devices);

  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      "id1",
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update state from connected to connecting, this should have no effect.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      "id1", /*connection_state=*/mojom::DeviceConnectionState::kConnecting));
  SetPairedDevices(devices);

  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      "id1",
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from connecting to connected.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties("id1"));
  SetPairedDevices(devices);

  ASSERT_EQ(2u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      "id1",
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());
}

TEST_F(BluetoothDeviceStatusNotifierImplTest, DisconnectToStopObserving) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();

  // Initially, observer would receive no updates.
  ASSERT_EQ(0u, observer->paired_device_properties_list().size());
  EXPECT_TRUE(observer->paired_device_properties_list().empty());

  // Disconnect the Mojo pipe; this should stop observing.
  observer->DisconnectMojoPipe();

  // Add a paired device; the observer should not be notified since it
  // is no longer connected.
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices;
  paired_devices.push_back(GenerateStubPairedDeviceProperties("id1"));
  EXPECT_EQ(0u, observer->paired_device_properties_list().size());
}

}  // namespace bluetooth_config
}  // namespace chromeos
