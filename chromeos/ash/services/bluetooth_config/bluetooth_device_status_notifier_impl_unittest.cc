// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/bluetooth_device_status_notifier_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_device_status_observer.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_client_uuids.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

const uint32_t kTestBluetoothClass = 1337u;

// Used for devices which are not connected by a nearby share connection.
const char kNonNearbyUuid[] = "00001108-0000-1000-8000-00805f9b34fb";

mojom::PairedBluetoothDevicePropertiesPtr GenerateStubPairedDeviceProperties(
    const std::string& id,
    mojom::DeviceConnectionState connection_state =
        mojom::DeviceConnectionState::kConnected) {
  auto device_properties = mojom::BluetoothDeviceProperties::New();

  // Concate "-Identifier" so device id matches
  // MockBluetoothDevice->GetIdentifier() return value.
  device_properties->id = base::StrCat({id, "-Identifier"});
  device_properties->address = "test_device_address";
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
  BluetoothDeviceStatusNotifierImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  BluetoothDeviceStatusNotifierImplTest(
      const BluetoothDeviceStatusNotifierImplTest&) = delete;
  BluetoothDeviceStatusNotifierImplTest& operator=(
      const BluetoothDeviceStatusNotifierImplTest&) = delete;
  ~BluetoothDeviceStatusNotifierImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetDevices())
        .WillByDefault(testing::Invoke(
            this, &BluetoothDeviceStatusNotifierImplTest::GenerateDevices));

    device_status_notifier_ =
        std::make_unique<BluetoothDeviceStatusNotifierImpl>(
            mock_adapter_, &fake_device_cache_,
            chromeos::FakePowerManagerClient::Get());
  }

  void TearDown() override {
    // Destroy |device_status_notifier_| before the fake power manager client in
    // order to remove observers correctly.
    device_status_notifier_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  // Pass in a list of ids because MockBluetoothDevice appends "-Identifier"
  // to any value passed in as identifier. We want GetIdentifier() value
  // returned from MockBluetoothDevice to be the same as the id in
  // PairedBluetoothDevicePropertiesPtr. The ids passed in here do not include
  // "-Identifier" prefix.
  void SetPairedDevices(
      const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
          paired_devices,
      const std::vector<std::string>& ids,
      const bool includes_nearby_uuids = false) {
    EXPECT_EQ(paired_devices.size(), ids.size());

    std::vector<mojom::PairedBluetoothDevicePropertiesPtr> copy;
    mock_devices_.clear();

    base::flat_set<device::BluetoothUUID> uuid_set;

    if (includes_nearby_uuids) {
      uuid_set.insert(nearby::GetNearbyClientUuids().front());
    } else {
      uuid_set.insert({device::BluetoothUUID(kNonNearbyUuid)});
    }

    for (unsigned int i = 0; i < paired_devices.size(); ++i) {
      copy.push_back(paired_devices[i].Clone());

      bool connected = paired_devices[i]->device_properties->connection_state ==
                       mojom::DeviceConnectionState::kConnected;
      auto mock_device =
          std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
              mock_adapter_.get(), kTestBluetoothClass,
              base::UTF16ToASCII(
                  paired_devices[i]->device_properties->public_name)
                  .c_str(),
              ids[i], /*paired=*/true, connected);

      ON_CALL(*mock_device, GetUUIDs())
          .WillByDefault(testing::Return(uuid_set));

      mock_devices_.push_back(std::move(mock_device));
    }

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

  int64_t GetSuspendCooldownTimeoutSeconds() {
    return BluetoothDeviceStatusNotifierImpl::kSuspendCooldownTimeout
        .InSeconds();
  }

  void FastForwardBy(int64_t seconds) {
    task_environment_.FastForwardBy(base::Seconds(seconds));
  }

 private:
  std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
  GenerateDevices() {
    std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
        devices;
    for (auto& device : mock_devices_)
      devices.push_back(device.get());
    return devices;
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>>
      mock_devices_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  FakeAdapterStateController fake_adapter_state_controller_;
  FakeDeviceCache fake_device_cache_{&fake_adapter_state_controller_};
  std::unique_ptr<BluetoothDeviceStatusNotifierImpl> device_status_notifier_;
};

TEST_F(BluetoothDeviceStatusNotifierImplTest, PairedDevicesChanges) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();

  // Initially, observer would receive no updates.
  ASSERT_EQ(0u, observer->paired_device_properties_list().size());
  EXPECT_TRUE(observer->paired_device_properties_list().empty());
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  EXPECT_TRUE(observer->disconnected_device_properties_list().empty());

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices;
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id",
      /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));

  // Add a paired but disconnected device and verify that the observer was
  // not notified.
  SetPairedDevices(paired_devices, /*ids=*/{"id"});
  ASSERT_EQ(0u, observer->paired_device_properties_list().size());
  EXPECT_TRUE(observer->paired_device_properties_list().empty());
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());

  paired_devices.pop_back();
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id1",
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));

  // Remove the paired, disconnected device and add a paired, connected device
  // and verify that only the paired list observer was notified.
  SetPairedDevices(paired_devices, /*ids=*/{"id1"});
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());
  ASSERT_EQ(
      "id1-Identifier",
      observer->paired_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());

  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id2",
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id3",
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));

  // Add two paired devices and verify that the observer was notified.
  SetPairedDevices(paired_devices, /*ids=*/{"id1", "id2", "id3"});
  ASSERT_EQ(3u, observer->paired_device_properties_list().size());
  ASSERT_EQ(
      "id2-Identifier",
      observer->paired_device_properties_list()[1]->device_properties->id);
  ASSERT_EQ(
      "id3-Identifier",
      observer->paired_device_properties_list()[2]->device_properties->id);
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());

  // Simulate device being unpaired, the observer should be notified the device
  // was disconnected.
  paired_devices.pop_back();
  SetPairedDevices(paired_devices, /*ids=*/{"id1", "id2"});
  ASSERT_EQ(3u, observer->paired_device_properties_list().size());
  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());

  // Add the same device again.
  paired_devices.push_back(GenerateStubPairedDeviceProperties(
      "id3",
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));

  // Verify that the observer was notified.
  SetPairedDevices(paired_devices, /*ids=*/{"id1", "id2", "id3"});
  ASSERT_EQ(4u, observer->paired_device_properties_list().size());
  ASSERT_EQ(
      "id3-Identifier",
      observer->paired_device_properties_list()[3]->device_properties->id);
  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
}

TEST_F(BluetoothDeviceStatusNotifierImplTest, ConnectedDevicesChanges) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> devices;
  devices.push_back(GenerateStubPairedDeviceProperties("id1"));

  // Add a connected device
  SetPairedDevices(devices, /*ids=*/{"id1"});

  // Initially, observer would receive no updates.
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  EXPECT_TRUE(observer->disconnected_device_properties_list().empty());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  EXPECT_TRUE(observer->connected_device_properties_list().empty());

  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from connected to disconnected.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      "id1",
      /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));

  SetPairedDevices(devices, /*ids=*/{"id1"});

  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ("id1-Identifier", observer->disconnected_device_properties_list()[0]
                                  ->device_properties->id);
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from disconnected to connected.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties("id1"));
  SetPairedDevices(devices, /*ids=*/{"id1"});

  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      "id1-Identifier",
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update state from connected to connecting, this should have no effect.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      "id1",
      /*connection_state=*/mojom::DeviceConnectionState::kConnecting));
  SetPairedDevices(devices, /*ids=*/{"id1"});

  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      "id1-Identifier",
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from connecting to connected.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties("id1"));
  SetPairedDevices(devices, /*ids=*/{"id1"});

  ASSERT_EQ(2u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      "id1-Identifier",
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

TEST_F(BluetoothDeviceStatusNotifierImplTest,
       DeviceDisconnectsConnectsDuringSuspend) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();
  const std::string device_id = "id1";

  // Add a connected device
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> devices;
  devices.push_back(GenerateStubPairedDeviceProperties(device_id));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Simulate device being suspended from lid closing.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::LID_CLOSED);

  // Update device from connected to disconnected. Observers should not be
  // notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from disconnected to connected. Observers should be notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      base::StrCat({device_id, "-Identifier"}),
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());
}

TEST_F(BluetoothDeviceStatusNotifierImplTest,
       DeviceDisconnectsConnectsAfterSuspendDuringCooldown) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();
  const std::string device_id = "id1";

  // Add a connected device
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> devices;
  devices.push_back(GenerateStubPairedDeviceProperties(device_id));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Simulate Chromebook being suspended from lid closing.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::LID_CLOSED);

  // Simulate Chromebook being awakened.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone(
      base::Milliseconds(1000));

  // Update device from connected to disconnected. Observers should not be
  // notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from disconnected to connected. Observers should be notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      base::StrCat({device_id, "-Identifier"}),
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());
}

TEST_F(BluetoothDeviceStatusNotifierImplTest,
       DeviceDisconnectsConnectsAfterSuspendCooldown) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();
  const std::string device_id = "id1";

  // Add a connected device
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> devices;
  devices.push_back(GenerateStubPairedDeviceProperties(device_id));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Simulate Chromebook being suspended from lid closing.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::LID_CLOSED);

  // Simulate Chromebook being awakened.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone(
      base::Milliseconds(1000));

  // Simulate the suspend cooldown passing.
  FastForwardBy(GetSuspendCooldownTimeoutSeconds());

  // Update device from connected to disconnected. Observers should be notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(base::StrCat({device_id, "-Identifier"}),
            observer->disconnected_device_properties_list()[0]
                ->device_properties->id);
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from disconnected to connected. Observers should be notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(1u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      base::StrCat({device_id, "-Identifier"}),
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());
}

TEST_F(BluetoothDeviceStatusNotifierImplTest,
       DeviceDisconnectsConnectsDuringSecondSuspend) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();
  const std::string device_id = "id1";

  // Add a connected device
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> devices;
  devices.push_back(GenerateStubPairedDeviceProperties(device_id));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Simulate Chromebook being suspended from lid closing.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::LID_CLOSED);

  // Simulate Chromebook being awakened.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone(
      base::Milliseconds(1000));

  // Simulate |seconds_forward| seconds passing.
  int seconds_forward = 1;
  FastForwardBy(seconds_forward);

  // Simulate Chromebook being suspended from lid closing again.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::LID_CLOSED);

  // Simulate suspend cooldown time passing. This is where the first timer
  // should timeout if it wasn't canceled.
  FastForwardBy(GetSuspendCooldownTimeoutSeconds() - seconds_forward);

  // Update device from connected to disconnected. Observers should not be
  // notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());

  // Update device from disconnected to connected. Observers should be notified.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      device_id,
      /*connection_state=*/mojom::DeviceConnectionState::kConnected));
  SetPairedDevices(devices, /*ids=*/{device_id});

  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(1u, observer->connected_device_properties_list().size());
  ASSERT_EQ(
      base::StrCat({device_id, "-Identifier"}),
      observer->connected_device_properties_list()[0]->device_properties->id);
  ASSERT_EQ(1u, observer->paired_device_properties_list().size());
}

TEST_F(BluetoothDeviceStatusNotifierImplTest, NearbyDevicePaired) {
  std::unique_ptr<FakeBluetoothDeviceStatusObserver> observer = Observe();

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> devices;
  devices.push_back(GenerateStubPairedDeviceProperties("id1"));
  SetPairedDevices(devices, /*ids=*/{"id1"}, /*is_nearby_devices=*/true);

  // Initially, observer would receive no updates.
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(0u, observer->paired_device_properties_list().size());

  // Update device from connected to disconnected.
  devices.pop_back();
  devices.push_back(GenerateStubPairedDeviceProperties(
      "id1",
      /*connection_state=*/mojom::DeviceConnectionState::kNotConnected));

  SetPairedDevices(devices, /*ids=*/{"id1"}, /*is_nearby_devices=*/true);

  // No notifications are shown because the device is connected using
  // Nearby connections.
  ASSERT_EQ(0u, observer->disconnected_device_properties_list().size());
  ASSERT_EQ(0u, observer->connected_device_properties_list().size());
  ASSERT_EQ(0u, observer->paired_device_properties_list().size());
}

}  // namespace ash::bluetooth_config
