// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestAddress[] = "11:22:33:44:55:66";

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(device::MockBluetoothAdapter* adapter,
                      const std::string& address)
      : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                       /*bluetooth_class=*/0u,
                                                       /*name=*/"Test Device",
                                                       address,
                                                       /*paired=*/true,
                                                       /*connected=*/true) {}

  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;
};

}  // namespace

namespace nearby::chrome {

class BleV2GattClientTest : public testing::Test {
 public:
  BleV2GattClientTest() = default;
  ~BleV2GattClientTest() override = default;
  BleV2GattClientTest(const BleV2GattClientTest&) = delete;
  BleV2GattClientTest& operator=(const BleV2GattClientTest&) = delete;

  void SetUp() override {
    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    fake_device_ =
        std::make_unique<FakeBluetoothDevice>(adapter_.get(), kTestAddress);

    ON_CALL(*adapter_, GetDevice(kTestAddress))
        .WillByDefault(Return(fake_device_.get()));

    auto connection = std::make_unique<
        testing::NiceMock<device::MockBluetoothGattConnection>>(adapter_,
                                                                kTestAddress);

    // TODO(b/316395226): We're creating a real Device object here, and relying
    // on the underlying MockBluetoothDevice implementation to handle the test
    // logic. This is likely to become unwieldy, and we should define and use a
    // stubbed FakeDevice class instead.
    mojo::PendingRemote<bluetooth::mojom::Device> pending_device;
    bluetooth::Device::Create(adapter_, std::move(connection),
                              pending_device.InitWithNewPipeAndPassReceiver());

    ble_v2_gatt_client_ =
        std::make_unique<BleV2GattClient>(std::move(pending_device));
  }

  void TearDown() override {
    ble_v2_gatt_client_->Disconnect();

    // TODO(b/316395226): Rework to avoid RunUntilIdle().
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<FakeBluetoothDevice> fake_device_;
  std::unique_ptr<BleV2GattClient> ble_v2_gatt_client_;
};

// TODO(b/311430390): Remove this skeleton test once other methods are
// implemented.
TEST_F(BleV2GattClientTest, SetUpSucceeds) {
  // SetUp() should instantiate the gatt client.
  EXPECT_FALSE(!ble_v2_gatt_client_);
}

}  // namespace nearby::chrome
