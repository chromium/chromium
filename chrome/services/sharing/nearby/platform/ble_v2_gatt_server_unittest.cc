// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_server.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class BleV2GattServerTest : public testing::Test {
 public:
  BleV2GattServerTest() = default;
  ~BleV2GattServerTest() override = default;
  BleV2GattServerTest(const BleV2GattServerTest&) = delete;
  BleV2GattServerTest& operator=(const BleV2GattServerTest&) = delete;

  void SetUp() override {
    auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
    fake_adapter_ = fake_adapter.get();
    mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_adapter),
        pending_adapter.InitWithNewPipeAndPassReceiver());
    remote_adapter_.Bind(std::move(pending_adapter),
                         /*bind_task_runner=*/nullptr);

    ble_v2_gatt_server_ = std::make_unique<BleV2GattServer>(remote_adapter_);
  }

 protected:
  raw_ptr<bluetooth::FakeAdapter> fake_adapter_ = nullptr;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<BleV2GattServer> ble_v2_gatt_server_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BleV2GattServerTest, GetBlePeripheral) {
  BluetoothAdapter& ble_v2_periphral = ble_v2_gatt_server_->GetBlePeripheral();
  EXPECT_EQ(fake_adapter_->address_, ble_v2_periphral.GetAddress());
}

}  // namespace nearby::chrome
