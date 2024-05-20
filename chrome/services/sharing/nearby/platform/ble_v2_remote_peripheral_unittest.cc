// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

const char kAddress[] = "11:12:13:14:15:16";
constexpr uint64_t kUniqueId = 24279786918417;

}  // namespace

class BleV2RemotePeripheralTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BleV2RemotePeripheralTest, GetAddress) {
  auto device_info = bluetooth::mojom::DeviceInfo::New();
  device_info->address = kAddress;
  BleV2RemotePeripheral peripheral{std::move(device_info)};
  EXPECT_EQ(peripheral.GetAddress(), kAddress);
}

TEST_F(BleV2RemotePeripheralTest, GetUniqueId) {
  auto device_info = bluetooth::mojom::DeviceInfo::New();
  device_info->address = kAddress;
  BleV2RemotePeripheral peripheral{std::move(device_info)};
  EXPECT_EQ(kUniqueId, peripheral.GetUniqueId());
}

TEST_F(BleV2RemotePeripheralTest, CanUpdateWithSameAddress) {
  auto device_info = bluetooth::mojom::DeviceInfo::New();
  device_info->address = kAddress;
  BleV2RemotePeripheral peripheral{std::move(device_info)};
  EXPECT_EQ(peripheral.GetAddress(), kAddress);
  auto device_info2 = bluetooth::mojom::DeviceInfo::New();
  device_info2->address = kAddress;
  peripheral.UpdateDeviceInfo(std::move(device_info2));
  EXPECT_EQ(peripheral.GetAddress(), kAddress);
}

TEST_F(BleV2RemotePeripheralTest, CanNotUpdateWithDifferentAddress) {
  auto device_info = bluetooth::mojom::DeviceInfo::New();
  device_info->address = kAddress;
  BleV2RemotePeripheral peripheral{std::move(device_info)};
  EXPECT_EQ(peripheral.GetAddress(), kAddress);
  auto device_info2 = bluetooth::mojom::DeviceInfo::New();
  device_info2->address = "different address";
  EXPECT_DCHECK_DEATH(peripheral.UpdateDeviceInfo(std::move(device_info2)));
}

}  // namespace nearby::chrome
