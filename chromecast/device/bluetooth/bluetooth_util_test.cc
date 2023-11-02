// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace bluetooth {
namespace util {

TEST(BluetoothUtilTest, AddrStringConversion) {
  const char kBadAddr1[] = "foo";
  const char kBadAddr2[] = "aa:bb:cc:dd:ee:ag";

  const char kGoodAddr1[] = "aa:bb:cc:dd:ee:ff";
  const char kGoodAddr2[] = "AA:BB:CC:DD:EE:FF";
  const char kGoodAddr3[] = "A1:B2:C3:D4:E5:F6";
  const char kGoodAddr4[] = "a1:b2:c3:d4:e5:f6";

  const bluetooth_v2_shlib::Addr kGoodBytes1 = {
      {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa}};
  const bluetooth_v2_shlib::Addr kGoodBytes3 = {
      {0xf6, 0xe5, 0xd4, 0xc3, 0xb2, 0xa1}};

  bluetooth_v2_shlib::Addr addr;
  EXPECT_FALSE(ParseAddr(kBadAddr1, &addr));
  EXPECT_FALSE(ParseAddr(kBadAddr2, &addr));

  EXPECT_TRUE(ParseAddr(kGoodAddr1, &addr));
  EXPECT_EQ(kGoodBytes1, addr);
  EXPECT_EQ(kGoodAddr1, AddrToString(addr));

  EXPECT_TRUE(ParseAddr(kGoodAddr2, &addr));
  EXPECT_EQ(kGoodBytes1, addr);
  EXPECT_EQ(kGoodAddr1, AddrToString(addr));

  EXPECT_TRUE(ParseAddr(kGoodAddr3, &addr));
  EXPECT_EQ(kGoodBytes3, addr);
  EXPECT_EQ(kGoodAddr4, AddrToString(addr));
}

TEST(BluetoothUtilTest, UuidStringConversion) {
  const char kBadUuid1[] = "hello";
  const char kBadUuid2[] = "a822c885-af02-c780-9d4d-bd9a1fa06d9z";
  const char kBadUuid3[] = "00000000-0000-0000-0000-0x0000000000";
  const char kBadUuid4[] = "123e-567-e89b-12d3-a456-426655440000";
  const char kBadUuid5[] = "123e456--e89b-12d3-a456-426655440000";
  const char kBadUuid6[] = "123e4567--e89b-12d3-a456-426655440000";

  const char kUuid1[] = "123e4567-e89b-12d3-a456-426655440000";
  const char kUuid2[] = "123E4567-E89B-12D3-A456-426655440000";
  const char kUuid3[] = "a822c885-af02-c780-9d4d-bd9a1fa06d93";
  const char kUuid4[] = "FE34";
  const char kUuid5[] = "a822c885af02c7809d4dbd9a1fa06d93";

  const bluetooth_v2_shlib::Uuid kGoodBytes1 = {
      {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
       0x55, 0x44, 0x00, 0x00}};
  const bluetooth_v2_shlib::Uuid kGoodBytes3 = {
      {0xa8, 0x22, 0xc8, 0x85, 0xaf, 0x02, 0xc7, 0x80, 0x9d, 0x4d, 0xbd, 0x9a,
       0x1f, 0xa0, 0x6d, 0x93}};

  const bluetooth_v2_shlib::Uuid kGoodBytes4 = {
      {0x00, 0x00, 0xfe, 0x34, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80,
       0x5F, 0x9B, 0x34, 0xFB}};

  bluetooth_v2_shlib::Uuid uuid;
  EXPECT_FALSE(ParseUuid(kBadUuid1, &uuid));
  EXPECT_FALSE(ParseUuid(kBadUuid2, &uuid));
  EXPECT_FALSE(ParseUuid(kBadUuid3, &uuid));
  EXPECT_FALSE(ParseUuid(kBadUuid4, &uuid));
  EXPECT_FALSE(ParseUuid(kBadUuid5, &uuid));
  EXPECT_FALSE(ParseUuid(kBadUuid6, &uuid));

  EXPECT_TRUE(ParseUuid(kUuid1, &uuid));
  EXPECT_EQ(kGoodBytes1, uuid);
  EXPECT_EQ(kUuid1, UuidToString(uuid));

  EXPECT_TRUE(ParseUuid(kUuid2, &uuid));
  EXPECT_EQ(kGoodBytes1, uuid);
  EXPECT_EQ(kUuid1, UuidToString(uuid));

  EXPECT_TRUE(ParseUuid(kUuid3, &uuid));
  EXPECT_EQ(kGoodBytes3, uuid);
  EXPECT_EQ(kUuid3, UuidToString(uuid));

  EXPECT_TRUE(ParseUuid(kUuid4, &uuid));
  EXPECT_EQ(kGoodBytes4, uuid);

  EXPECT_TRUE(ParseUuid(kUuid5, &uuid));
  EXPECT_EQ(kGoodBytes3, uuid);
  EXPECT_EQ(kUuid3, UuidToString(uuid));
}

TEST(BluetoothUtilTest, UuidFromInt16) {
  static const bluetooth_v2_shlib::Uuid kExpected = {
      {0x00, 0x00, 0x11, 0x0a, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80,
       0x5F, 0x9B, 0x34, 0xFB}};
  EXPECT_EQ(kExpected, UuidFromInt16(0x110a));
}

TEST(BluetoothUtilTest, UuidFromInt32) {
  static const bluetooth_v2_shlib::Uuid kExpected = {
      {0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80,
       0x5F, 0x9B, 0x34, 0xFB}};
  EXPECT_EQ(kExpected, UuidFromInt32(0x01020304));
}

}  // namespace util
}  // namespace bluetooth
}  // namespace chromecast
