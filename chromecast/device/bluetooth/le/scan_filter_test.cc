// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/scan_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace bluetooth {

namespace {

const char kName[] = "foo";
const bluetooth_v2_shlib::Uuid kUuid = {{0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b,
                                         0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
                                         0x55, 0x44, 0x00, 0x00}};

}  // namespace

TEST(ScanFilterTest, Name) {
  ScanFilter filter;
  LeScanResult result;
  EXPECT_TRUE(filter.Matches(result));

  filter.name = kName;
  EXPECT_FALSE(filter.Matches(result));

  result.type_to_data[LeScanResult::kGapShortName].emplace_back(
      reinterpret_cast<const uint8_t*>(kName),
      reinterpret_cast<const uint8_t*>(kName) + strlen(kName));
  EXPECT_TRUE(filter.Matches(result));

  ++result.type_to_data[LeScanResult::kGapShortName][0][0];
  EXPECT_FALSE(filter.Matches(result));
}

TEST(ScanFilterTest, Uuid) {
  ScanFilter filter;
  filter.service_uuid = kUuid;

  LeScanResult result;
  EXPECT_FALSE(filter.Matches(result));

  result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids]
      .emplace_back(kUuid.rbegin(), kUuid.rend());
  EXPECT_TRUE(filter.Matches(result));

  ++result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids][0][0];
  EXPECT_FALSE(filter.Matches(result));
}

TEST(ScanFilterTest, NameAndUuid) {
  ScanFilter filter;
  filter.name = kName;
  filter.service_uuid = kUuid;

  LeScanResult result;
  EXPECT_FALSE(filter.Matches(result));

  result.type_to_data[LeScanResult::kGapShortName].emplace_back(
      reinterpret_cast<const uint8_t*>(kName),
      reinterpret_cast<const uint8_t*>(kName) + strlen(kName));
  EXPECT_FALSE(filter.Matches(result));

  result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids]
      .emplace_back(kUuid.rbegin(), kUuid.rend());
  EXPECT_TRUE(filter.Matches(result));

  // Unmatching name shouldn't work.
  --result.type_to_data[LeScanResult::kGapShortName][0][0];
  EXPECT_FALSE(filter.Matches(result));

  // Unmatching uuid shouldn't work.
  ++result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids][0][0];
  ++result.type_to_data[LeScanResult::kGapShortName][0][0];
  EXPECT_FALSE(filter.Matches(result));

  --result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids][0][0];
  EXPECT_TRUE(filter.Matches(result));
}

TEST(ScanFilterTest, RegexName) {
  const char kHello[] = "hello";

  // Just test some basic regular experssions, we don't want this to be testing
  // RE2.
  ScanFilter filter;
  filter.regex_name = "ell";

  LeScanResult result;
  EXPECT_FALSE(filter.Matches(result));

  result.type_to_data[LeScanResult::kGapShortName].emplace_back(
      reinterpret_cast<const uint8_t*>(kHello),
      reinterpret_cast<const uint8_t*>(kHello) + strlen(kHello));
  EXPECT_TRUE(filter.Matches(result));

  filter.regex_name = "g";
  EXPECT_FALSE(filter.Matches(result));

  filter.regex_name = "h.*o";
  EXPECT_TRUE(filter.Matches(result));
}

TEST(ScanFilterTest, RegexNameIgnoredIfNameSet) {
  ScanFilter filter;
  filter.regex_name = ".";  // Match any string.
  filter.name = "bar";

  LeScanResult result;
  result.type_to_data[LeScanResult::kGapShortName].emplace_back(
      reinterpret_cast<const uint8_t*>(kName),
      reinterpret_cast<const uint8_t*>(kName) + strlen(kName));
  EXPECT_FALSE(filter.Matches(result));

  filter.name = kName;
  EXPECT_TRUE(filter.Matches(result));
}

}  // namespace bluetooth
}  // namespace chromecast
