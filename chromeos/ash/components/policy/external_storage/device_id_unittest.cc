// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/external_storage/device_id.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chromeos/ash/components/disks/disk.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(DeviceIdTest, FromDict) {
  // clang-format off
  const struct TestData {
    std::string_view json_str;
    std::optional<DeviceId> expected_device_id;
  } kTestData[] = {
    {R"({"vendor_id": 1234, "product_id": 5678})", DeviceId{1234, 5678}},
    {R"({"vendor_id": 39612, "product_id": 57072})", DeviceId{0x9abc, 0xdef0}},
    {"10", std::nullopt},
    {R"({"product_id": 123})", std::nullopt},
    {R"({"vendor_id": "456"})", std::nullopt},
    {R"({"vendor_id": -1, "product_id": 456})", std::nullopt},
    {R"({"vendor_id": 123, "product_id": 65536})", std::nullopt},
  };
  // clang-format on

  for (const auto& t : kTestData) {
    std::optional<base::Value> value = base::JSONReader::Read(t.json_str);
    ASSERT_TRUE(value.has_value());
    std::optional<DeviceId> device_id = DeviceId::FromDict(value.value());
    SCOPED_TRACE(testing::Message() << "value: " << t.json_str);
    EXPECT_EQ(device_id, t.expected_device_id);
  }
}

TEST(DeviceIdTest, FromUint32) {
  // clang-format off
  const struct TestData {
    uint32_t vid;
    uint32_t pid;
    std::optional<DeviceId> expected_device_id;
  } kTestData[] = {
    {1234, 5678, DeviceId{1234, 5678}},
    {0xABCD, 0, DeviceId{0xABCD, 0}},
    {0, 0x1A, DeviceId{0, 0x1A}},
    {0x12345, 123, std::nullopt},
    {456, 0x12345678, std::nullopt},
  };
  // clang-format on

  for (const auto& t : kTestData) {
    std::optional<DeviceId> device_id = DeviceId::FromUint32(t.vid, t.pid);
    SCOPED_TRACE(testing::Message() << "vid: " << t.vid << ", pid: " << t.pid);
    EXPECT_EQ(device_id, t.expected_device_id);
  }
}

TEST(DeviceIdTest, FromVidPid) {
  // clang-format off
  const struct TestData {
    std::string_view vid;
    std::string_view pid;
    std::optional<DeviceId> expected_device_id;
  } kTestData[] = {
    {"1234", "5678", DeviceId{0x1234, 0x5678}},
    {"0xABcd", "0", DeviceId{0xABCD, 0}},
    {"0", "1A", DeviceId{0, 0x1A}},
    {"12345", "123", std::nullopt},
    {"456", "12345678", std::nullopt},
    {"123", "123456789", std::nullopt},
    {"-456", "1234", std::nullopt},
    {" 123", "456", std::nullopt},
    {"", "456", std::nullopt},
    {"123", "fgh", std::nullopt},
  };
  // clang-format on

  for (const auto& t : kTestData) {
    std::optional<DeviceId> device_id = DeviceId::FromVidPid(t.vid, t.pid);
    SCOPED_TRACE(testing::Message() << "vid: " << t.vid << ", pid: " << t.pid);
    EXPECT_EQ(device_id, t.expected_device_id);
  }
}

TEST(DeviceIdTest, FromDisk) {
  using ash::disks::Disk;
  // clang-format off
  const struct TestData {
    std::unique_ptr<Disk> disk;
    std::optional<DeviceId> expected_device_id;
  } kTestData[] = {
    {nullptr, std::nullopt},
    {Disk::Builder().Build(), std::nullopt},
    {Disk::Builder().SetVendorId("1234").Build(), std::nullopt},
    {Disk::Builder().SetProductId("5678").Build(), std::nullopt},
    {Disk::Builder().SetVendorId("1234").SetProductId("5678").Build(),
     DeviceId{0x1234, 0x5678}},
  };
  // clang-format on

  int i = 0;
  for (const auto& t : kTestData) {
    std::optional<DeviceId> device_id = DeviceId::FromDisk(t.disk.get());
    EXPECT_EQ(device_id, t.expected_device_id) << "Failed test case #" << i;
    i++;
  }
}

}  // namespace policy
