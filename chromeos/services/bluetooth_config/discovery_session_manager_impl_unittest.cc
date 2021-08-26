// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/discovery_session_manager_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_discovery_delegate.h"
#include "chromeos/services/bluetooth_config/fake_device_cache.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

using StartScanCallback = base::OnceCallback<void(
    /*is_error=*/bool,
    device::UMABluetoothDiscoverySessionOutcome)>;
using StopScanCallback =
    device::BluetoothAdapter::DiscoverySessionResultCallback;

mojom::BluetoothDevicePropertiesPtr GenerateStubDeviceProperties(
    const std::string id = "device_id") {
  auto device_properties = mojom::BluetoothDeviceProperties::New();
  device_properties->id = id;
  device_properties->public_name = u"name";
  device_properties->device_type = mojom::DeviceType::kUnknown;
  device_properties->audio_capability =
      mojom::AudioOutputCapability::kNotCapableOfAudioOutput;
  device_properties->connection_state =
      mojom::DeviceConnectionState::kNotConnected;
  return device_properties;
}

}  // namespace

class DiscoverySessionManagerImplTest : public testing::Test {
 protected:
  DiscoverySessionManagerImplTest() = default;
  DiscoverySessionManagerImplTest(const DiscoverySessionManagerImplTest&) =
      delete;
  DiscoverySessionManagerImplTest& operator=(
      const DiscoverySessionManagerImplTest&) = delete;
  ~DiscoverySessionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, StartScanWithFilter_(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](const device::BluetoothDiscoveryFilter* filter,
                   StartScanCallback& callback) {
              EXPECT_FALSE(start_scan_callback_);
              start_scan_callback_ = std::move(callback);
            }));
    ON_CALL(*mock_adapter_, StopScan(testing::_))
        .WillByDefault(testing::Invoke([this](StopScanCallback callback) {
          EXPECT_FALSE(stop_scan_callback_);
          stop_scan_callback_ = std::move(callback);
        }));

    discovery_session_manager_ = std::make_unique<DiscoverySessionManagerImpl>(
        &fake_adapter_state_controller_, mock_adapter_, &fake_device_cache_);
  }

  std::unique_ptr<FakeBluetoothDiscoveryDelegate> StartDiscovery() {
    auto delegate = std::make_unique<FakeBluetoothDiscoveryDelegate>();
    discovery_session_manager_->StartDiscovery(
        delegate->GeneratePendingRemote());

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(delegate->IsMojoPipeConnected());

    return delegate;
  }

  bool HasPendingStartScanCallback() const {
    return !start_scan_callback_.is_null();
  }

  void InvokePendingStartScanCallback(bool success) {
    std::move(start_scan_callback_)
        .Run(!success,
             success ? device::UMABluetoothDiscoverySessionOutcome::SUCCESS
                     : device::UMABluetoothDiscoverySessionOutcome::FAILED);
    base::RunLoop().RunUntilIdle();
  }

  bool HasPendingStopScanCallback() const {
    return !stop_scan_callback_.is_null();
  }

  void InvokePendingStopScanCallback(bool success) {
    std::move(stop_scan_callback_)
        .Run(!success,
             success ? device::UMABluetoothDiscoverySessionOutcome::SUCCESS
                     : device::UMABluetoothDiscoverySessionOutcome::FAILED);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateDiscoverySessionStopping() {
    DiscoverySessionManagerImpl* impl =
        static_cast<DiscoverySessionManagerImpl*>(
            discovery_session_manager_.get());
    impl->AdapterDiscoveringChanged(mock_adapter_.get(), /*discovering=*/false);
    base::RunLoop().RunUntilIdle();
  }

  void SetBluetoothSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
  }

  void SetUnpairedDevices(
      const std::vector<mojom::BluetoothDevicePropertiesPtr>&
          unpaired_devices) {
    std::vector<mojom::BluetoothDevicePropertiesPtr> copy;
    for (const auto& unpaired_device : unpaired_devices)
      copy.push_back(unpaired_device.Clone());

    fake_device_cache_.SetUnpairedDevices(std::move(copy));
    discovery_session_manager_->FlushForTesting();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  StartScanCallback start_scan_callback_;
  StopScanCallback stop_scan_callback_;

  FakeAdapterStateController fake_adapter_state_controller_;
  FakeDeviceCache fake_device_cache_{&fake_adapter_state_controller_};
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<DiscoverySessionManager> discovery_session_manager_;
};

TEST_F(DiscoverySessionManagerImplTest, StartDiscoveryThenDisconnectToStop) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());
  EXPECT_TRUE(delegate->discovered_devices_list().empty());

  // Add an unpaired device and verify that the delegate was notified.
  std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices;
  unpaired_devices.push_back(GenerateStubDeviceProperties(/*id=*/"device_id1"));
  SetUnpairedDevices(unpaired_devices);
  EXPECT_EQ(1u, delegate->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[0]->id,
            delegate->discovered_devices_list()[0]->id);

  // Add another unpaired device and verify that the delegate was notified.
  unpaired_devices.push_back(GenerateStubDeviceProperties(/*id=*/"device_id2"));
  SetUnpairedDevices(unpaired_devices);
  EXPECT_EQ(2u, delegate->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[1]->id,
            delegate->discovered_devices_list()[1]->id);

  // Disconnect the Mojo pipe; this should trigger a StopScan() call.
  delegate->DisconnectMojoPipe();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());

  // Invoke the StopScan() callback. Since the delegate was already
  // disconnected, it should not have received a callback.
  InvokePendingStopScanCallback(/*success=*/true);
  EXPECT_EQ(0u, delegate->num_stop_callbacks());

  // Add another unpaired device; the delegate should not be notified.
  unpaired_devices.push_back(GenerateStubDeviceProperties(/*id=*/"device_id2"));
  SetUnpairedDevices(unpaired_devices);
  EXPECT_EQ(2u, delegate->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[0]->id,
            delegate->discovered_devices_list()[0]->id);
}

TEST_F(DiscoverySessionManagerImplTest, FailToStart) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  // Fail to start scanning.
  InvokePendingStartScanCallback(/*success=*/false);
  EXPECT_EQ(0u, delegate->num_start_callbacks());

  // We expect that another request was made to retry scanning; succeed this
  // time.
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_EQ(1u, delegate->num_start_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, DisconnectBeforeFailureToStart) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  // Disconnect the Mojo pipe, before the discovery session starts.
  delegate->DisconnectMojoPipe();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(0u, delegate->num_start_callbacks());

  // Invoke the pending callback to start the scan. Since the delegate was
  // already disconnected, it should not have receivd a callback.
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_EQ(0u, delegate->num_start_callbacks());

  // Since there is no longer a client, we should have attempted to stop the
  // discovery session. Invoke the StopScan() callback; since the delegate was
  // already disconnected, it should not have received a callback.
  InvokePendingStopScanCallback(/*success=*/true);
  EXPECT_EQ(0u, delegate->num_stop_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, UnexpectedlyStoppedDiscovering) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());

  // Simulate the discovery session stopping unexpectedly. The delegate should
  // become disconnected.
  SimulateDiscoverySessionStopping();
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_stop_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, BluetoothTurnsOff) {
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();

  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_start_callbacks());

  // Start disabling Bluetooth; the delegate should become disconnected.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabling);
  EXPECT_FALSE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->num_stop_callbacks());
}

TEST_F(DiscoverySessionManagerImplTest, MultipleClients) {
  // Add the first client and start discovery.
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate1 = StartDiscovery();
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate1->num_start_callbacks());
  EXPECT_TRUE(delegate1->discovered_devices_list().empty());

  // Add an unpaired device and verify that the first client was notified.
  std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices;
  unpaired_devices.push_back(GenerateStubDeviceProperties());
  SetUnpairedDevices(unpaired_devices);
  EXPECT_EQ(1u, delegate1->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[0]->id,
            delegate1->discovered_devices_list()[0]->id);

  // Add a second client; it should reuse the existing discovery session, and no
  // new pending request should have been created. It should immediately be
  // notified of the current discovered devices list.
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate2 = StartDiscovery();
  EXPECT_TRUE(delegate2->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate2->num_start_callbacks());
  EXPECT_FALSE(HasPendingStartScanCallback());
  EXPECT_EQ(1u, delegate2->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[0]->id,
            delegate2->discovered_devices_list()[0]->id);

  // Disconnect the first client; since the second client is still active, there
  // should be no pending StopScan() call.
  delegate1->DisconnectMojoPipe();
  EXPECT_FALSE(HasPendingStopScanCallback());

  // Add another unpaired device; the second client should be notified but the
  // first client should not.
  unpaired_devices.push_back(GenerateStubDeviceProperties());
  SetUnpairedDevices(unpaired_devices);
  EXPECT_EQ(1u, delegate1->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[0]->id,
            delegate1->discovered_devices_list()[0]->id);
  EXPECT_EQ(2u, delegate2->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[1]->id,
            delegate2->discovered_devices_list()[1]->id);

  // Disconnect the second client; now that there are no remaining clients,
  // StopScan() should have been called.
  delegate2->DisconnectMojoPipe();
  InvokePendingStopScanCallback(/*success=*/true);
}

TEST_F(DiscoverySessionManagerImplTest, DiscoverDeviceBeforeStart) {
  // Add an unpaired device.
  std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices;
  unpaired_devices.push_back(GenerateStubDeviceProperties());
  SetUnpairedDevices(unpaired_devices);

  // Add a client and start discovery. The client should be notified of the
  // current discovered devices list once discovery has started.
  std::unique_ptr<FakeBluetoothDiscoveryDelegate> delegate = StartDiscovery();
  InvokePendingStartScanCallback(/*success=*/true);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(1u, delegate->discovered_devices_list().size());
  EXPECT_EQ(unpaired_devices[0]->id,
            delegate->discovered_devices_list()[0]->id);

  delegate->DisconnectMojoPipe();
  InvokePendingStopScanCallback(/*success=*/true);
}

}  // namespace bluetooth_config
}  // namespace chromeos
