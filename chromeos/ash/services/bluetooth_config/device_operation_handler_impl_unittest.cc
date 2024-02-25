// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_operation_handler_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_name_manager.h"
#include "chromeos/ash/services/bluetooth_config/fake_fast_pair_delegate.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";
const char kTestBluetoothNickname[] = "testNickname";
const base::TimeDelta kTestDuration = base::Milliseconds(1000);

}  // namespace

class DeviceOperationHandlerImplTest : public testing::Test {
 protected:
  using Operation = DeviceOperationHandler::Operation;

  DeviceOperationHandlerImplTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DeviceOperationHandlerImplTest(const DeviceOperationHandlerImplTest&) =
      delete;
  DeviceOperationHandlerImplTest& operator=(
      const DeviceOperationHandlerImplTest&) = delete;
  ~DeviceOperationHandlerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetDevices())
        .WillByDefault(testing::Invoke(
            this, &DeviceOperationHandlerImplTest::GetMockDevices));

    device_operation_handler_ = std::make_unique<DeviceOperationHandlerImpl>(
        &fake_adapter_state_controller_, mock_adapter_,
        &fake_device_name_manager_, &fake_fast_pair_delegate_);
  }

  void SetBluetoothSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
  }

  void AssertHistogramMetrics(int success_count, int failure_count) {
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result",
        /*success=*/false, failure_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result",
        /*success=*/true, success_count);
  }

  void AssertDurationHistogramMetrics(base::TimeDelta bucket,
                                      int success_count,
                                      int failure_count,
                                      std::string transport_name) {
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.Success",
        bucket.InMilliseconds(), success_count);
    histogram_tester.ExpectBucketCount(
        base::StrCat({"Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt."
                      "Duration.Success.",
                      transport_name}),
        bucket.InMilliseconds(), success_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.Failure",
        bucket.InMilliseconds(), failure_count);
    histogram_tester.ExpectBucketCount(
        base::StrCat({"Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt."
                      "Duration.Failure.",
                      transport_name}),
        bucket.InMilliseconds(), failure_count);
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

    ON_CALL(*mock_device, ConnectClassic(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](device::BluetoothDevice::PairingDelegate* pairing_delegate,
                   device::BluetoothDevice::ConnectCallback callback) {
              EXPECT_FALSE(connect_callback_);
              connect_callback_ = std::move(callback);
            }));
    ON_CALL(*mock_device, Disconnect(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](base::OnceClosure callback,
                   device::BluetoothDevice::ErrorCallback error_callback) {
              EXPECT_FALSE(disconnect_callbacks_.has_value());
              disconnect_callbacks_ = std::make_pair(std::move(callback),
                                                     std::move(error_callback));
            }));
    ON_CALL(*mock_device, Forget(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](base::OnceClosure callback,
                   device::BluetoothDevice::ErrorCallback error_callback) {
              EXPECT_FALSE(forget_callbacks_.has_value());
              forget_callbacks_ = std::make_pair(std::move(callback),
                                                 std::move(error_callback));
            }));
    ON_CALL(*mock_device, GetType()).WillByDefault(testing::Invoke([]() {
      return device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC;
    }));

    mock_devices_.push_back(std::move(mock_device));
  }

  void ConnectDevice(const std::string& device_id) {
    device_operation_handler_->Connect(
        device_id,
        base::BindOnce(&DeviceOperationHandlerImplTest::OnOperationFinished,
                       base::Unretained(this), device_id, Operation::kConnect));
  }

  void DisconnectDevice(const std::string& device_id) {
    device_operation_handler_->Disconnect(
        device_id,
        base::BindOnce(&DeviceOperationHandlerImplTest::OnOperationFinished,
                       base::Unretained(this), device_id,
                       Operation::kDisconnect));
  }

  void ForgetDevice(const std::string& device_id) {
    device_operation_handler_->Forget(
        device_id,
        base::BindOnce(&DeviceOperationHandlerImplTest::OnOperationFinished,
                       base::Unretained(this), device_id, Operation::kForget));
  }

  void OnOperationFinished(const std::string& device_id,
                           Operation operation,
                           bool success) {
    results_.emplace_back(device_id, operation, success);
  }

  base::TimeDelta GetOperationTimeout() {
    return DeviceOperationHandler::kOperationTimeout;
  }

  void FastForwardOperation(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  const std::vector<std::tuple<std::string, Operation, bool>>& results() {
    return results_;
  }

  bool HasPendingConnectCallback() const {
    return !connect_callback_.is_null();
  }

  void InvokePendingConnectCallback(bool success) {
    if (success) {
      std::move(connect_callback_).Run(std::nullopt);
    } else {
      std::move(connect_callback_)
          .Run(device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    }
  }

  bool HasPendingDisconnectCallback() const {
    return disconnect_callbacks_.has_value();
  }

  void InvokePendingDisconnectCallback(bool success) {
    if (success) {
      std::move(disconnect_callbacks_->first).Run();
    } else {
      std::move(disconnect_callbacks_->second).Run();
    }
    disconnect_callbacks_.reset();
  }

  bool HasPendingForgetCallback() const {
    return forget_callbacks_.has_value();
  }

  void InvokePendingForgetCallback(bool success) {
    if (success) {
      std::move(forget_callbacks_->first).Run();
    } else {
      std::move(forget_callbacks_->second).Run();
    }
    forget_callbacks_.reset();
  }

  void SetDeviceNickname(const std::string& device_id) {
    fake_device_name_manager_.SetDeviceNickname(device_id,
                                                kTestBluetoothNickname);
  }

  std::vector<std::string> GetFastPairDeletedDevices() {
    return fake_fast_pair_delegate_.forgotten_device_addresses();
  }

  std::optional<std::string> GetDeviceNickname(const std::string& device_id) {
    return fake_device_name_manager_.GetDeviceNickname(device_id);
  }

  base::HistogramTester histogram_tester;

 private:
  std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
  GetMockDevices() {
    std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
        devices;
    for (auto& device : mock_devices_)
      devices.push_back(device.get());
    return devices;
  }

  base::test::TaskEnvironment task_environment_;

  // Results of processed operations. Each entry contains the operation's
  // device ID, which Operation it was, and if it succeeded or not.
  std::vector<std::tuple<std::string, Operation, bool>> results_;

  device::BluetoothDevice::ConnectCallback connect_callback_;
  std::optional<
      std::pair<base::OnceClosure, device::BluetoothDevice::ErrorCallback>>
      disconnect_callbacks_;
  std::optional<
      std::pair<base::OnceClosure, device::BluetoothDevice::ErrorCallback>>
      forget_callbacks_;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  FakeAdapterStateController fake_adapter_state_controller_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  FakeDeviceNameManager fake_device_name_manager_;
  FakeFastPairDelegate fake_fast_pair_delegate_;

  std::unique_ptr<DeviceOperationHandlerImpl> device_operation_handler_;
};

TEST_F(DeviceOperationHandlerImplTest,
       ConnectBluetoothDisabledThenNotFoundThenFailThenSucceed) {
  std::string device_id = "testid";

  // Connect should fail due to Bluetooth being disabled.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);
  ConnectDevice(device_id);
  EXPECT_EQ(results()[0], std::make_tuple(device_id, Operation::kConnect,
                                          /*success=*/false));
  AssertHistogramMetrics(/*success_count=*/0, /*failure_count=*/1);
  AssertDurationHistogramMetrics(base::Milliseconds(0), /*success_count=*/0,
                                 /*failure_count=*/1,
                                 /*transport_name=*/"Invalid");

  // Connect should fail due to device not being found.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kEnabled);
  ConnectDevice(device_id);
  EXPECT_EQ(results()[1],
            std::make_tuple(device_id, Operation::kConnect, false));
  AssertHistogramMetrics(/*success_count=*/0, /*failure_count=*/2);
  AssertDurationHistogramMetrics(base::Milliseconds(0), /*success_count=*/0,
                                 /*failure_count=*/2,
                                 /*transport_name=*/"Invalid");

  // Add the device and simulate BluetoothDevice::Connect() failing.
  AddDevice(&device_id);
  ConnectDevice(device_id);
  FastForwardOperation(kTestDuration);
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/false);
  EXPECT_EQ(results()[2], std::make_tuple(device_id, Operation::kConnect,
                                          /*success=*/false));

  AssertHistogramMetrics(/*success_count=*/0, /*failure_count=*/3);
  AssertDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                 /*failure_count=*/1,
                                 /*transport_name=*/"Classic");

  // Simulate BluetoothDevice::Connect() succeeding.
  ConnectDevice(device_id);
  EXPECT_TRUE(HasPendingConnectCallback());
  FastForwardOperation(kTestDuration);
  InvokePendingConnectCallback(/*success=*/true);
  EXPECT_EQ(results()[3], std::make_tuple(device_id, Operation::kConnect,
                                          /*success=*/true));
  AssertHistogramMetrics(/*success_count=*/1, /*failure_count=*/3);
  AssertDurationHistogramMetrics(kTestDuration, /*success_count=*/1,
                                 /*failure_count=*/1,
                                 /*transport_name=*/"Classic");
}

TEST_F(DeviceOperationHandlerImplTest, DisconnectNotFoundFailThenSucceed) {
  std::string device_id = "testid";

  // Disconnect should fail due to device not being found.
  DisconnectDevice(device_id);
  EXPECT_EQ(results()[0], std::make_tuple(device_id, Operation::kDisconnect,
                                          /*success=*/false));

  // Add the device and simulate BluetoothDevice::Disconnect() failing.
  AddDevice(&device_id);
  DisconnectDevice(device_id);
  EXPECT_TRUE(HasPendingDisconnectCallback());
  InvokePendingDisconnectCallback(/*success=*/false);
  EXPECT_EQ(results()[1], std::make_tuple(device_id, Operation::kDisconnect,
                                          /*success=*/false));

  // Simulate BluetoothDevice::Disconnect() succeeding.
  DisconnectDevice(device_id);
  EXPECT_TRUE(HasPendingDisconnectCallback());
  InvokePendingDisconnectCallback(/*success=*/true);
  EXPECT_EQ(results()[2], std::make_tuple(device_id, Operation::kDisconnect,
                                          /*success=*/true));
}

TEST_F(DeviceOperationHandlerImplTest, ForgetNotFoundThenSucceed) {
  std::string device_id = "testid";

  // Forget should fail due to device not being found.
  ForgetDevice(device_id);
  EXPECT_EQ(results()[0], std::make_tuple(device_id, Operation::kForget,
                                          /*success=*/false));

  // Add and forget the device.
  AddDevice(&device_id);
  ForgetDevice(device_id);

  // Forgetting a device will never fail, and the handler will immediately
  // notify that the operation finished successfully, so don't bother checking
  // for pending callbacks.
  EXPECT_EQ(results()[1], std::make_tuple(device_id, Operation::kForget,
                                          /*success=*/true));
}

TEST_F(DeviceOperationHandlerImplTest, ForgettingDeviceCallsFastPairDelegate) {
  std::string device_id;
  AddDevice(&device_id);
  EXPECT_EQ(GetFastPairDeletedDevices().size(), 0u);

  ForgetDevice(device_id);

  EXPECT_EQ(results()[0],
            std::make_tuple(device_id, Operation::kForget, /*success=*/true));
  EXPECT_EQ(GetFastPairDeletedDevices().size(), 1u);

  // Derive the address from the device ID.
  std::string address = device_id.substr(0, device_id.find("-Identifier"));
  EXPECT_EQ(GetFastPairDeletedDevices()[0], address);
}

TEST_F(DeviceOperationHandlerImplTest, ForgettingDeviceRemovesNickname) {
  std::string device_id;
  AddDevice(&device_id);

  SetDeviceNickname(device_id);
  std::optional<std::string> nickname = GetDeviceNickname(device_id);
  EXPECT_TRUE(nickname.has_value());
  EXPECT_EQ(kTestBluetoothNickname, nickname.value());

  ForgetDevice(device_id);
  EXPECT_EQ(results()[0],
            std::make_tuple(device_id, Operation::kForget, /*success=*/true));
  EXPECT_FALSE(GetDeviceNickname(device_id).has_value());
}

TEST_F(DeviceOperationHandlerImplTest, SimultaneousOperationsAreQueued) {
  std::string device_id1 = "device_id1";
  AddDevice(&device_id1);
  std::string device_id2 = "device_id2";
  AddDevice(&device_id2);
  std::string device_id3 = "device_id3";
  AddDevice(&device_id3);

  // Connect to the first device. BluetoothDevice::Connect() should be
  // called.
  ConnectDevice(device_id1);
  EXPECT_TRUE(HasPendingConnectCallback());

  // Attempt to disconnect another device. BluetoothDevice::Disconnect() should
  // not be called yet.
  DisconnectDevice(device_id2);
  EXPECT_FALSE(HasPendingDisconnectCallback());
  FastForwardOperation(kTestDuration);

  // Invoke the first connect callback.
  InvokePendingConnectCallback(/*success=*/false);
  EXPECT_EQ(results()[0], std::make_tuple(device_id1, Operation::kConnect,
                                          /*success=*/false));
  AssertHistogramMetrics(/*success_count=*/0, /*failure_count=*/1);
  AssertDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                 /*failure_count=*/1,
                                 /*transport_name=*/"Classic");

  // Now the second operation's BluetoothDevice::Disconnect() should have been
  // called.
  EXPECT_TRUE(HasPendingDisconnectCallback());

  // Attempt to forget a third device and disable Bluetooth.
  // BluetoothDevice::Forget() should not be called yet,
  ForgetDevice(device_id3);
  EXPECT_FALSE(HasPendingForgetCallback());
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);

  // Succeed with the disconnect call.
  InvokePendingDisconnectCallback(/*success=*/true);
  EXPECT_EQ(results()[1], std::make_tuple(device_id2, Operation::kDisconnect,
                                          /*success=*/true));

  // The forget call should immediately fail due to Bluetooth being disabled.
  EXPECT_EQ(results()[2], std::make_tuple(device_id3, Operation::kForget,
                                          /*success=*/false));
}

TEST_F(DeviceOperationHandlerImplTest, OperationsTimeout) {
  std::string device_id1 = "device_id1";
  AddDevice(&device_id1);
  std::string device_id2 = "device_id2";
  AddDevice(&device_id2);

  // Connect to the first device. BluetoothDevice::Connect() should be
  // called.
  ConnectDevice(device_id1);
  EXPECT_TRUE(HasPendingConnectCallback());

  // Queue disconnecting the second device.
  DisconnectDevice(device_id2);
  EXPECT_FALSE(HasPendingDisconnectCallback());

  // Simulate connect timing out. The operation should finish with a failure
  // result and the disconnect started.
  FastForwardOperation(GetOperationTimeout());
  EXPECT_EQ(results()[0], std::make_tuple(device_id1, Operation::kConnect,
                                          /*success=*/false));
  EXPECT_TRUE(HasPendingDisconnectCallback());
  AssertHistogramMetrics(/*success_count=*/0, /*failure_count=*/1);
  AssertDurationHistogramMetrics(GetOperationTimeout(), /*success_count=*/0,
                                 /*failure_count=*/1,
                                 /*transport_name=*/"Classic");

  // Simulate disconnect timing out. The operation should finish with a failure
  // result.
  FastForwardOperation(GetOperationTimeout());
  EXPECT_EQ(results()[1], std::make_tuple(device_id2, Operation::kDisconnect,
                                          /*success=*/false));
}

TEST_F(DeviceOperationHandlerImplTest, OperationCompletesBeforeTimeout) {
  std::string device_id = "testid";
  AddDevice(&device_id);

  // Connect to device.
  ConnectDevice(device_id);
  FastForwardOperation(kTestDuration);
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/true);
  EXPECT_EQ(results()[0], std::make_tuple(device_id, Operation::kConnect,
                                          /*success=*/true));
  AssertHistogramMetrics(/*success_count=*/1, /*failure_count=*/0);
  AssertDurationHistogramMetrics(kTestDuration, /*success_count=*/1,
                                 /*failure_count=*/0,
                                 /*transport_name=*/"Classic");

  // Fast forward to where the timer would timeout if it was still running. This
  // will crash if the timer isn't cancelled after the operation finished.
  FastForwardOperation(GetOperationTimeout());
}

TEST_F(DeviceOperationHandlerImplTest, OperationCompletesAfterTimeout) {
  std::string device_id1 = "device_id1";
  AddDevice(&device_id1);
  std::string device_id2 = "device_id2";
  AddDevice(&device_id2);

  // Connect to the first device. BluetoothDevice::Connect() should be
  // called.
  ConnectDevice(device_id1);
  EXPECT_TRUE(HasPendingConnectCallback());

  // Queue disconnecting the second device.
  DisconnectDevice(device_id2);
  EXPECT_FALSE(HasPendingDisconnectCallback());

  // Simulate connect timing out. The operation should finish with a failure
  // result and the disconnect started.
  FastForwardOperation(GetOperationTimeout());
  EXPECT_EQ(results()[0], std::make_tuple(device_id1, Operation::kConnect,
                                          /*success=*/false));
  EXPECT_TRUE(HasPendingDisconnectCallback());
  AssertHistogramMetrics(/*success_count=*/0, /*failure_count=*/1);
  AssertDurationHistogramMetrics(GetOperationTimeout(), /*success_count=*/0,
                                 /*failure_count=*/1,
                                 /*transport_name=*/"Classic");

  // Simulate the connect call now finishing after the timeout. This should not
  // cause the current operation (disconnect) to finish with a result.
  InvokePendingConnectCallback(/*success=*/true);
  EXPECT_EQ(results().size(), 1u);

  // Finish the disconnect operation.
  InvokePendingDisconnectCallback(/*success=*/true);
  EXPECT_EQ(results()[1], std::make_tuple(device_id2, Operation::kDisconnect,
                                          /*success=*/true));
  AssertHistogramMetrics(/*success_count=*/0, /*failure_count=*/1);
  AssertDurationHistogramMetrics(GetOperationTimeout(), /*success_count=*/0,
                                 /*failure_count=*/1,
                                 /*transport_name=*/"Classic");
}

}  // namespace ash::bluetooth_config
