// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"

#include <atomic>
#include <thread>
#include <vector>

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

TEST_F(BleV2RemotePeripheralTest, ConcurrentGetAddressAndUpdateDeviceInfo) {
  auto device_info = bluetooth::mojom::DeviceInfo::New();
  device_info->address = kAddress;
  BleV2RemotePeripheral peripheral{std::move(device_info)};

  std::atomic<bool> stop{false};
  std::vector<std::thread> readers;
  for (int i = 0; i < 4; ++i) {
    readers.emplace_back([&]() {
      while (!stop.load(std::memory_order_relaxed)) {
        EXPECT_EQ(peripheral.GetAddress(), kAddress);
        EXPECT_EQ(peripheral.GetUniqueId(), kUniqueId);
      }
    });
  }

  for (int i = 0; i < 5000; ++i) {
    auto updated_info = bluetooth::mojom::DeviceInfo::New();
    updated_info->address = kAddress;
    peripheral.UpdateDeviceInfo(std::move(updated_info));
  }

  stop.store(true, std::memory_order_relaxed);
  for (auto& reader : readers) {
    reader.join();
  }
}

}  // namespace nearby::chrome
