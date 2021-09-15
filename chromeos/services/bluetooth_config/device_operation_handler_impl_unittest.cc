// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_operation_handler_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";

}  // namespace

class DeviceOperationHandlerImplTest : public testing::Test {
 protected:
  DeviceOperationHandlerImplTest() = default;
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
        &fake_adapter_state_controller_, mock_adapter_);
  }

  void SetBluetoothSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
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

    ON_CALL(*mock_device, Connect_(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](device::BluetoothDevice::PairingDelegate* pairing_delegate,
                   device::BluetoothDevice::ConnectCallback& callback) {
              EXPECT_FALSE(connect_callback_);
              connect_callback_ = std::move(callback);
            }));
    mock_devices_.push_back(std::move(mock_device));
  }

  void ConnectDevice(const std::string& device_id) {
    device_operation_handler_->Connect(
        device_id,
        base::BindOnce(&DeviceOperationHandlerImplTest::OnOperationFinished,
                       base::Unretained(this)));
  }

  void OnOperationFinished(bool success) { results_.push_back(success); }

  const std::vector<bool>& results() { return results_; }

  bool HasPendingConnectCallback() const {
    return !connect_callback_.is_null();
  }

  void InvokePendingConnectCallback(bool success) {
    if (success) {
      std::move(connect_callback_).Run(absl::nullopt);
    } else {
      std::move(connect_callback_)
          .Run(device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    }
  }

 private:
  std::vector<const device::BluetoothDevice*> GetMockDevices() {
    std::vector<const device::BluetoothDevice*> devices;
    for (auto& device : mock_devices_)
      devices.push_back(device.get());
    return devices;
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<bool> results_;

  device::BluetoothDevice::ConnectCallback connect_callback_;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  FakeAdapterStateController fake_adapter_state_controller_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<DeviceOperationHandlerImpl> device_operation_handler_;
};

TEST_F(DeviceOperationHandlerImplTest,
       ConnectBluetoothDisabledThenNotFoundThenFailThenSucceed) {
  std::string device_id = "testid";

  // Connect should fail due to Bluetooth being disabled.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);
  ConnectDevice(device_id);
  EXPECT_FALSE(results()[0]);

  // Connect should fail due to device not being found.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kEnabled);
  ConnectDevice(device_id);
  EXPECT_FALSE(results()[1]);

  // Add the device and simulate BluetoothDevice::Connect() failing.
  AddDevice(&device_id);
  ConnectDevice(device_id);
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/false);
  EXPECT_FALSE(results()[2]);

  // Simulate BluetoothDevice::Connect() succeeding.
  ConnectDevice(device_id);
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/true);
  EXPECT_TRUE(results()[3]);
}

TEST_F(DeviceOperationHandlerImplTest, SimultaneousOperationsAreQueued) {
  std::string device_id1;
  AddDevice(&device_id1);
  std::string device_id2;
  AddDevice(&device_id2);

  // Connect to the first device. BluetoothDevice::Connect() should be
  // called.
  ConnectDevice(device_id1);
  EXPECT_TRUE(HasPendingConnectCallback());

  // Attempt to connect another device. BluetoothDevice::Connect() should not be
  // called.
  ConnectDevice(device_id2);

  // Invoke first connect callback.
  InvokePendingConnectCallback(/*success=*/false);
  EXPECT_EQ(results()[0], false);

  // Now the second BluetoothDevice::Connect() should have been called.
  EXPECT_TRUE(HasPendingConnectCallback());

  // Attempt to connect to the first device again and disable Bluetooth.
  ConnectDevice(device_id1);
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);

  // Succeed with the second connect call.
  InvokePendingConnectCallback(/*success=*/true);
  EXPECT_EQ(results()[1], true);

  // The third connect should immediately fail due to Bluetooth being disabled.
  EXPECT_EQ(results()[2], false);

  // TODO(gordonseto): Test Forget and Disconnect calls here when they're
  // implemented.
}

}  // namespace bluetooth_config
}  // namespace chromeos
