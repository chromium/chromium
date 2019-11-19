// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_monitor_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/components/proximity_auth/proximity_monitor_observer.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/secure_channel/fake_connection.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothDevice;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;

namespace proximity_auth {
namespace {

const char kRemoteDeviceUserId[] = "example@gmail.com";
const char kRemoteDeviceName[] = "LGE Nexus 5";
const int kRssiThreshold = -70;

class MockProximityMonitorObserver : public ProximityMonitorObserver {
 public:
  MockProximityMonitorObserver() {}
  ~MockProximityMonitorObserver() override {}

  MOCK_METHOD0(OnProximityStateChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityMonitorObserver);
};

// Creates a mock Bluetooth adapter and sets it as the global adapter for
// testing.
scoped_refptr<device::MockBluetoothAdapter>
CreateAndRegisterMockBluetoothAdapter() {
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new NiceMock<device::MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);
  return adapter;
}

}  // namespace

class ProximityAuthProximityMonitorImplTest : public testing::Test {
 public:
  ProximityAuthProximityMonitorImplTest()
      : bluetooth_adapter_(CreateAndRegisterMockBluetoothAdapter()),
        remote_bluetooth_device_(&*bluetooth_adapter_,
                                 0,
                                 kRemoteDeviceName,
                                 "",
                                 false /* paired */,
                                 true /* connected */),
        fake_client_channel_(
            std::make_unique<chromeos::secure_channel::FakeClientChannel>()),
        remote_device_(chromeos::multidevice::RemoteDeviceRefBuilder()
                           .SetUserId(kRemoteDeviceUserId)
                           .SetName(kRemoteDeviceName)
                           .Build()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  ~ProximityAuthProximityMonitorImplTest() override {}

  void InitializeTest(bool multidevice_flags_enabled) {
    fake_multidevice_setup_client_ = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();

    monitor_ = std::make_unique<ProximityMonitorImpl>(
        remote_device_, fake_client_channel_.get());

    ON_CALL(*bluetooth_adapter_, GetDevice(std::string()))
        .WillByDefault(Return(&remote_bluetooth_device_));
    ON_CALL(remote_bluetooth_device_, GetConnectionInfo(_))
        .WillByDefault(SaveArg<0>(&connection_info_callback_));
    monitor_->AddObserver(&observer_);
  }

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

  void ProvideRssi(base::Optional<int32_t> rssi) {
    RunPendingTasks();

    std::vector<chromeos::secure_channel::mojom::ConnectionCreationDetail>
        creation_details{
            chromeos::secure_channel::mojom::ConnectionCreationDetail::
                REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING};

    chromeos::secure_channel::mojom::BluetoothConnectionMetadataPtr
        bluetooth_connection_metadata_ptr;
    if (rssi) {
      bluetooth_connection_metadata_ptr =
          chromeos::secure_channel::mojom::BluetoothConnectionMetadata::New(
              *rssi);
    }

    chromeos::secure_channel::mojom::ConnectionMetadataPtr
        connection_metadata_ptr =
            chromeos::secure_channel::mojom::ConnectionMetadata::New(
                creation_details, std::move(bluetooth_connection_metadata_ptr),
                "channel_binding_data");
    fake_client_channel_->InvokePendingGetConnectionMetadataCallback(
        std::move(connection_metadata_ptr));
  }

 protected:
  // Mock for verifying interactions with the proximity monitor's observer.
  NiceMock<MockProximityMonitorObserver> observer_;

  // Mocks used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> bluetooth_adapter_;
  NiceMock<device::MockBluetoothDevice> remote_bluetooth_device_;
  std::unique_ptr<chromeos::secure_channel::FakeClientChannel>
      fake_client_channel_;
  chromeos::multidevice::RemoteDeviceRef remote_device_;
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

  // The proximity monitor under test.
  std::unique_ptr<ProximityMonitorImpl> monitor_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  BluetoothDevice::ConnectionInfoCallback connection_info_callback_;
  chromeos::multidevice::ScopedDisableLoggingForTesting disable_logging_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProximityAuthProximityMonitorImplTest, IsUnlockAllowed_NeverStarted) {
  InitializeTest(true /* multidevice_flags_enabled */);
  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_Started_NoRssiReceivedYet) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();
  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest, IsUnlockAllowed_RssiInRange) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();
  ProvideRssi(4);
  EXPECT_TRUE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest, IsUnlockAllowed_UnknownRssi) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();

  ProvideRssi(0);
  ProvideRssi(base::nullopt);

  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_InformsObserverOfChanges) {
  InitializeTest(true /* multidevice_flags_enabled */);

  // Initially, the device is not in proximity.
  monitor_->Start();
  EXPECT_FALSE(monitor_->IsUnlockAllowed());

  // Simulate receiving an RSSI reading in proximity.
  EXPECT_CALL(observer_, OnProximityStateChanged()).Times(1);
  ProvideRssi(kRssiThreshold / 2);
  EXPECT_TRUE(monitor_->IsUnlockAllowed());

  // Simulate a reading indicating non-proximity.
  EXPECT_CALL(observer_, OnProximityStateChanged()).Times(1);
  ProvideRssi(kRssiThreshold * 2);
  ProvideRssi(kRssiThreshold * 2);
  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest, IsUnlockAllowed_StartThenStop) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();

  ProvideRssi(0);
  EXPECT_TRUE(monitor_->IsUnlockAllowed());

  monitor_->Stop();
  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_StartThenStopThenStartAgain) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();
  ProvideRssi(kRssiThreshold / 2);
  ProvideRssi(kRssiThreshold / 2);
  ProvideRssi(kRssiThreshold / 2);
  ProvideRssi(kRssiThreshold / 2);
  ProvideRssi(kRssiThreshold / 2);
  EXPECT_TRUE(monitor_->IsUnlockAllowed());
  monitor_->Stop();

  // Restarting the monitor should immediately reset the proximity state, rather
  // than building on the previous rolling average.
  monitor_->Start();
  ProvideRssi(kRssiThreshold - 1);

  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_RemoteDeviceRemainsInProximity) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();
  ProvideRssi(kRssiThreshold / 2 + 1);
  ProvideRssi(kRssiThreshold / 2 - 1);
  ProvideRssi(kRssiThreshold / 2 + 2);
  ProvideRssi(kRssiThreshold / 2 - 3);

  EXPECT_TRUE(monitor_->IsUnlockAllowed());

  // Brief drops in RSSI should be handled by weighted averaging.
  ProvideRssi(kRssiThreshold - 5);

  EXPECT_TRUE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_RemoteDeviceLeavesProximity) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();

  // Start with a device in proximity.
  ProvideRssi(0);
  EXPECT_TRUE(monitor_->IsUnlockAllowed());

  // Simulate readings for the remote device leaving proximity.
  ProvideRssi(-1);
  ProvideRssi(-4);
  ProvideRssi(0);
  ProvideRssi(-10);
  ProvideRssi(-15);
  ProvideRssi(-20);
  ProvideRssi(kRssiThreshold);
  ProvideRssi(kRssiThreshold - 10);
  ProvideRssi(kRssiThreshold - 20);
  ProvideRssi(kRssiThreshold - 20);
  ProvideRssi(kRssiThreshold - 20);
  ProvideRssi(kRssiThreshold - 20);

  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_RemoteDeviceEntersProximity) {
  InitializeTest(true /* multidevice_flags_enabled */);
  monitor_->Start();

  // Start with a device out of proximity.
  ProvideRssi(kRssiThreshold * 2);
  EXPECT_FALSE(monitor_->IsUnlockAllowed());

  // Simulate readings for the remote device entering proximity.
  ProvideRssi(-15);
  ProvideRssi(-8);
  ProvideRssi(-12);
  ProvideRssi(-18);
  ProvideRssi(-7);
  ProvideRssi(-3);
  ProvideRssi(-2);
  ProvideRssi(0);
  ProvideRssi(0);

  EXPECT_TRUE(monitor_->IsUnlockAllowed());
}

// TODO(jhawkins): Fix this test.
TEST_F(ProximityAuthProximityMonitorImplTest,
       DISABLED_IsUnlockAllowed_DeviceNotKnownToAdapter) {
  InitializeTest(true /* multidevice_flags_enabled */);
  monitor_->Start();

  // Start with the device known to the adapter and in proximity.
  ProvideRssi(0);
  EXPECT_TRUE(monitor_->IsUnlockAllowed());

  // Simulate it being forgotten.
  ON_CALL(*bluetooth_adapter_, GetDevice(std::string()))
      .WillByDefault(Return(nullptr));
  EXPECT_CALL(observer_, OnProximityStateChanged());
  RunPendingTasks();

  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_DeviceNotConnected) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();

  // Start with the device connected and in proximity.
  ProvideRssi(0);
  EXPECT_TRUE(monitor_->IsUnlockAllowed());

  // Simulate it disconnecting.
  fake_client_channel_->NotifyDisconnected();
  EXPECT_CALL(observer_, OnProximityStateChanged());
  RunPendingTasks();

  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       IsUnlockAllowed_ConnectionInfoReceivedAfterStopping) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();
  monitor_->Stop();
  ProvideRssi(0);
  EXPECT_FALSE(monitor_->IsUnlockAllowed());
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       RecordProximityMetricsOnAuthSuccess_NormalValues) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();
  ProvideRssi(0);

  ProvideRssi(-20);

  base::HistogramTester histogram_tester;
  monitor_->RecordProximityMetricsOnAuthSuccess();
  histogram_tester.ExpectUniqueSample("EasyUnlock.AuthProximity.RollingRssi",
                                      -6, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.RemoteDeviceModelHash",
      1881443083 /* hash of "LGE Nexus 5" */, 1);
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       RecordProximityMetricsOnAuthSuccess_ClampedValues) {
  InitializeTest(true /* multidevice_flags_enabled */);

  monitor_->Start();
  ProvideRssi(-99999);

  base::HistogramTester histogram_tester;
  monitor_->RecordProximityMetricsOnAuthSuccess();
  histogram_tester.ExpectUniqueSample("EasyUnlock.AuthProximity.RollingRssi",
                                      -100, 1);
}

TEST_F(ProximityAuthProximityMonitorImplTest,
       RecordProximityMetricsOnAuthSuccess_UnknownValues) {
  InitializeTest(true /* multidevice_flags_enabled */);

  // Note: A device without a recorded name will have "Unknown" as its name.
  chromeos::multidevice::RemoteDeviceRef remote_device =
      chromeos::multidevice::RemoteDeviceRefBuilder()
          .SetUserId(kRemoteDeviceUserId)
          .SetName(std::string())
          .Build();

  ProximityMonitorImpl monitor(remote_device, fake_client_channel_.get());
  monitor.AddObserver(&observer_);
  monitor.Start();
  ProvideRssi(127);

  base::HistogramTester histogram_tester;
  monitor.RecordProximityMetricsOnAuthSuccess();
  histogram_tester.ExpectUniqueSample("EasyUnlock.AuthProximity.RollingRssi",
                                      127, 1);
  histogram_tester.ExpectUniqueSample(
      "EasyUnlock.AuthProximity.RemoteDeviceModelHash",
      -1808066424 /* hash of "Unknown" */, 1);
}

}  // namespace proximity_auth
