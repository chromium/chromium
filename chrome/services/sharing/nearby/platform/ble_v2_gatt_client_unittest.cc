// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class BleV2GattClientTest : public testing::Test {
 public:
  BleV2GattClientTest() = default;
  ~BleV2GattClientTest() override = default;
  BleV2GattClientTest(const BleV2GattClientTest&) = delete;
  BleV2GattClientTest& operator=(const BleV2GattClientTest&) = delete;

  void SetUp() override {
    ble_v2_gatt_client_ = std::make_unique<BleV2GattClient>();
  }

 protected:
  std::unique_ptr<BleV2GattClient> ble_v2_gatt_client_;
  base::test::TaskEnvironment task_environment_;
};

// TODO(b/311430390): Remove this skeleton test once other methods are
// implemented.
TEST_F(BleV2GattClientTest, SetUpSucceeds) {
  // SetUp() should instantiate the gatt client.
  EXPECT_FALSE(!ble_v2_gatt_client_);
}

}  // namespace nearby::chrome
