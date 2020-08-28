// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/ble_medium.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform_v2/ble_peripheral.h"
#include "chrome/services/sharing/nearby/platform_v2/bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {

const char kServiceName[] = "NearbySharing";

}  // namespace

class BleMediumTest : public testing::Test {
 public:
  BleMediumTest() = default;
  ~BleMediumTest() override = default;
  BleMediumTest(const BleMediumTest&) = delete;
  BleMediumTest& operator=(const BleMediumTest&) = delete;

  void SetUp() override { ble_medium_ = std::make_unique<BleMedium>(); }

 protected:
  std::unique_ptr<BleMedium> ble_medium_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BleMediumTest, TestAdvertising) {
  // TODO(b/154845685): Write test.
}

TEST_F(BleMediumTest, TestScanning) {
  // TODO(b/154848193): Write test.
}

TEST_F(BleMediumTest, TestStartAcceptingConnections) {
  // StartAcceptingConnections() should do nothing but still return true.
  EXPECT_TRUE(
      ble_medium_->StartAcceptingConnections(kServiceName, /*callback=*/{}));
}

TEST_F(BleMediumTest, TestConnect) {
  BluetoothDevice bluetooth_device(bluetooth::mojom::DeviceInfo::New());
  BlePeripheral ble_peripheral(bluetooth_device);

  // Connect() should do nothing and not return a valid api::BleSocket.
  EXPECT_FALSE(ble_medium_->Connect(ble_peripheral, kServiceName));
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
