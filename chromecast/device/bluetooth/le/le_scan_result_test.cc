// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/le_scan_result.h"

#include "base/containers/contains.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace bluetooth {

TEST(LeScanResultTest, SetAdvData) {
  static const uint8_t kBadData[] = {0x3, 0x2};

  LeScanResult scan_result;

  // Setting invalid data should not change the object.
  EXPECT_FALSE(scan_result.SetAdvData({kBadData}));
  EXPECT_TRUE(scan_result.adv_data.empty());
  EXPECT_TRUE(scan_result.type_to_data.empty());

  const std::vector<uint8_t> kFlags = {0x47};
  const std::vector<uint8_t> kName = {0x41, 0x42, 0x43};
  const std::vector<uint8_t> kService = {0x12, 0x34};

  std::vector<uint8_t> adv_data;
  adv_data.push_back(kFlags.size() + 1);
  adv_data.push_back(LeScanResult::kGapFlags);
  adv_data.insert(adv_data.end(), kFlags.begin(), kFlags.end());
  adv_data.push_back(kName.size() + 1);
  adv_data.push_back(LeScanResult::kGapCompleteName);
  adv_data.insert(adv_data.end(), kName.begin(), kName.end());
  adv_data.push_back(kService.size() + 1);
  adv_data.push_back(LeScanResult::kGapComplete16BitServiceUuids);
  adv_data.insert(adv_data.end(), kService.begin(), kService.end());

  EXPECT_TRUE(scan_result.SetAdvData(adv_data));
  EXPECT_EQ(adv_data, scan_result.adv_data);
  ASSERT_EQ(1ul, scan_result.type_to_data[LeScanResult::kGapFlags].size());
  EXPECT_EQ(kFlags, scan_result.type_to_data[LeScanResult::kGapFlags][0]);
  ASSERT_EQ(1ul,
            scan_result.type_to_data[LeScanResult::kGapCompleteName].size());
  EXPECT_EQ(kName, scan_result.type_to_data[LeScanResult::kGapCompleteName][0]);
  ASSERT_EQ(
      1ul, scan_result.type_to_data[LeScanResult::kGapComplete16BitServiceUuids]
               .size());
  EXPECT_EQ(
      kService,
      scan_result.type_to_data[LeScanResult::kGapComplete16BitServiceUuids][0]);
}

TEST(LeScanResultTest, Name) {
  LeScanResult scan_result;
  EXPECT_FALSE(scan_result.Name());

  static const char kName1[] = "foo";
  static const char kName2[] = "foobar";

  scan_result.type_to_data[LeScanResult::kGapShortName].push_back(
      std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kName1),
          reinterpret_cast<const uint8_t*>(kName1) + strlen(kName1)));
  std::optional<std::string> name = scan_result.Name();
  ASSERT_TRUE(name);
  EXPECT_EQ(kName1, *name);

  scan_result.type_to_data[LeScanResult::kGapCompleteName].push_back(
      std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kName2),
          reinterpret_cast<const uint8_t*>(kName2) + strlen(kName2)));

  name = scan_result.Name();
  ASSERT_TRUE(name);
  EXPECT_EQ(kName2, *name);
}

TEST(LeScanResultTest, Flags) {
  static const uint8_t kFlags = 0x42;

  LeScanResult scan_result;
  EXPECT_FALSE(scan_result.Flags());
  scan_result.type_to_data[LeScanResult::kGapFlags].push_back({kFlags});
  auto flags = scan_result.Flags();
  ASSERT_TRUE(flags);
  EXPECT_EQ(kFlags, *flags);
}

TEST(LeScanResultTest, AllUuids) {
  static const uint16_t kIncompleteUuid16 = 0x1234;
  const std::vector<uint8_t> kIncompleteUuid16Bytes = {0x34, 0x12};
  static const uint16_t kCompleteUuid16 = 0x5678;
  const std::vector<uint8_t> kCompleteUuid16Bytes = {0x78, 0x56};
  static const uint32_t kIncompleteUuid32 = 0x12345678;
  const std::vector<uint8_t> kIncompleteUuid32Bytes = {0x78, 0x56, 0x34, 0x12};
  static const uint32_t kCompleteUuid32 = 0xabcdef01;
  const std::vector<uint8_t> kCompleteUuid32Bytes = {0x1, 0xef, 0xcd, 0xab};
  static const bluetooth_v2_shlib::Uuid kIncompleteUuid128 = {
      {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
       0x55, 0x44, 0x00, 0x00}};
  static const bluetooth_v2_shlib::Uuid kCompleteUuid128 = {
      {0xa8, 0x22, 0xc8, 0x85, 0xaf, 0x02, 0xc7, 0x80, 0x9d, 0x4d, 0xbd, 0x9a,
       0x1f, 0xa0, 0x6d, 0x93}};
  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapIncomplete16BitServiceUuids]
      .push_back(kIncompleteUuid16Bytes);
  scan_result.type_to_data[LeScanResult::kGapComplete16BitServiceUuids]
      .push_back(kCompleteUuid16Bytes);
  scan_result.type_to_data[LeScanResult::kGapIncomplete32BitServiceUuids]
      .push_back(kIncompleteUuid32Bytes);
  scan_result.type_to_data[LeScanResult::kGapComplete32BitServiceUuids]
      .push_back(kCompleteUuid32Bytes);
  scan_result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids]
      .emplace_back(kIncompleteUuid128.rbegin(), kIncompleteUuid128.rend());
  scan_result.type_to_data[LeScanResult::kGapComplete128BitServiceUuids]
      .emplace_back(kCompleteUuid128.rbegin(), kCompleteUuid128.rend());

  auto all_uuids = scan_result.AllServiceUuids();
  ASSERT_TRUE(all_uuids);
  ASSERT_EQ(6ul, all_uuids->size());

  auto exists = [&all_uuids](const bluetooth_v2_shlib::Uuid& uuid) {
    return base::Contains(*all_uuids, uuid);
  };

  EXPECT_TRUE(exists(util::UuidFromInt16(kIncompleteUuid16)));
  EXPECT_TRUE(exists(util::UuidFromInt16(kCompleteUuid16)));
  EXPECT_TRUE(exists(util::UuidFromInt32(kIncompleteUuid32)));
  EXPECT_TRUE(exists(util::UuidFromInt32(kCompleteUuid32)));
  EXPECT_TRUE(exists(kIncompleteUuid128));
  EXPECT_TRUE(exists(kCompleteUuid128));
}

TEST(LeScanResultTest, IncompleteListOf16BitServiceUuid) {
  static const uint16_t kUuid1 = 0x1234;
  const std::vector<uint8_t> kUuid1Bytes = {0x34, 0x12};

  static const uint16_t kUuid2 = 0x5678;
  const std::vector<uint8_t> kUuid2Bytes = {0x78, 0x56};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapIncomplete16BitServiceUuids]
      .push_back(kUuid1Bytes);
  scan_result.type_to_data[LeScanResult::kGapIncomplete16BitServiceUuids]
      .push_back(kUuid2Bytes);

  auto uuids = scan_result.IncompleteListOf16BitServiceUuids();
  ASSERT_TRUE(uuids);
  ASSERT_EQ(2ul, uuids->size());
  EXPECT_EQ(util::UuidFromInt16(kUuid1), (*uuids)[0]);
  EXPECT_EQ(util::UuidFromInt16(kUuid2), (*uuids)[1]);
}

TEST(LeScanResultTest, CompleteListOf16BitServiceUuid) {
  static const uint16_t kUuid1 = 0x1234;
  const std::vector<uint8_t> kUuid1Bytes = {0x34, 0x12};

  static const uint16_t kUuid2 = 0x5678;
  const std::vector<uint8_t> kUuid2Bytes = {0x78, 0x56};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapComplete16BitServiceUuids]
      .push_back(kUuid1Bytes);
  scan_result.type_to_data[LeScanResult::kGapComplete16BitServiceUuids]
      .push_back(kUuid2Bytes);

  auto uuids = scan_result.CompleteListOf16BitServiceUuids();
  ASSERT_TRUE(uuids);
  ASSERT_EQ(2ul, uuids->size());
  EXPECT_EQ(util::UuidFromInt16(kUuid1), (*uuids)[0]);
  EXPECT_EQ(util::UuidFromInt16(kUuid2), (*uuids)[1]);
}

TEST(LeScanResultTest, IncompleteListOf32BitServiceUuid) {
  static const uint32_t kUuid1 = 0x12345678;
  const std::vector<uint8_t> kUuid1Bytes = {0x78, 0x56, 0x34, 0x12};

  static const uint32_t kUuid2 = 0xabcdef01;
  const std::vector<uint8_t> kUuid2Bytes = {0x1, 0xef, 0xcd, 0xab};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapIncomplete32BitServiceUuids]
      .push_back(kUuid1Bytes);
  scan_result.type_to_data[LeScanResult::kGapIncomplete32BitServiceUuids]
      .push_back(kUuid2Bytes);

  auto uuids = scan_result.IncompleteListOf32BitServiceUuids();
  ASSERT_TRUE(uuids);
  ASSERT_EQ(2ul, uuids->size());
  EXPECT_EQ(util::UuidFromInt32(kUuid1), (*uuids)[0]);
  EXPECT_EQ(util::UuidFromInt32(kUuid2), (*uuids)[1]);
}

TEST(LeScanResultTest, CompleteListOf32BitServiceUuid) {
  static const uint32_t kUuid1 = 0x12345678;
  const std::vector<uint8_t> kUuid1Bytes = {0x78, 0x56, 0x34, 0x12};

  static const uint32_t kUuid2 = 0xabcdef01;
  const std::vector<uint8_t> kUuid2Bytes = {0x1, 0xef, 0xcd, 0xab};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapComplete32BitServiceUuids]
      .push_back(kUuid1Bytes);
  scan_result.type_to_data[LeScanResult::kGapComplete32BitServiceUuids]
      .push_back(kUuid2Bytes);

  auto uuids = scan_result.CompleteListOf32BitServiceUuids();
  ASSERT_TRUE(uuids);
  ASSERT_EQ(2ul, uuids->size());
  EXPECT_EQ(util::UuidFromInt32(kUuid1), (*uuids)[0]);
  EXPECT_EQ(util::UuidFromInt32(kUuid2), (*uuids)[1]);
}

TEST(LeScanResultTest, IncompleteListOf128BitServiceUuid) {
  static const bluetooth_v2_shlib::Uuid kUuid1 = {
      {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
       0x55, 0x44, 0x00, 0x00}};
  static const bluetooth_v2_shlib::Uuid kUuid2 = {
      {0xa8, 0x22, 0xc8, 0x85, 0xaf, 0x02, 0xc7, 0x80, 0x9d, 0x4d, 0xbd, 0x9a,
       0x1f, 0xa0, 0x6d, 0x93}};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids]
      .emplace_back(kUuid1.rbegin(), kUuid1.rend());
  scan_result.type_to_data[LeScanResult::kGapIncomplete128BitServiceUuids]
      .emplace_back(kUuid2.rbegin(), kUuid2.rend());

  auto uuids = scan_result.IncompleteListOf128BitServiceUuids();
  ASSERT_TRUE(uuids);
  ASSERT_EQ(2ul, uuids->size());
  EXPECT_EQ(kUuid1, (*uuids)[0]);
  EXPECT_EQ(kUuid2, (*uuids)[1]);
}

TEST(LeScanResultTest, CompleteListOf128BitServiceUuid) {
  static const bluetooth_v2_shlib::Uuid kUuid1 = {
      {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
       0x55, 0x44, 0x00, 0x00}};
  static const bluetooth_v2_shlib::Uuid kUuid2 = {
      {0xa8, 0x22, 0xc8, 0x85, 0xaf, 0x02, 0xc7, 0x80, 0x9d, 0x4d, 0xbd, 0x9a,
       0x1f, 0xa0, 0x6d, 0x93}};
  static const bluetooth_v2_shlib::Uuid kUuid3 = {
      {0xaa, 0x22, 0xc8, 0x85, 0xaf, 0x02, 0xc7, 0x80, 0x9d, 0x4d, 0xbd, 0x9a,
       0x1f, 0xa0, 0x6d, 0x93}};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapComplete128BitServiceUuids]
      .emplace_back(kUuid1.rbegin(), kUuid1.rend());
  scan_result.type_to_data[LeScanResult::kGapComplete128BitServiceUuids]
      .emplace_back(kUuid2.rbegin(), kUuid2.rend());
  auto& end =
      scan_result.type_to_data[LeScanResult::kGapComplete128BitServiceUuids]
          .back();
  end.insert(end.end(), kUuid3.rbegin(), kUuid3.rend());

  auto uuids = scan_result.CompleteListOf128BitServiceUuids();
  ASSERT_TRUE(uuids);
  ASSERT_EQ(3ul, uuids->size());
  EXPECT_EQ(kUuid1, (*uuids)[0]);
  EXPECT_EQ(kUuid2, (*uuids)[1]);
  EXPECT_EQ(kUuid3, (*uuids)[2]);
}

TEST(LeScanResultTest, AllServiceData) {
  static const uint16_t kUuid16 = 0x1234;
  const std::vector<uint8_t> kUuid16Bytes = {0x34, 0x12, 0xab, 0xcd, 0xef};

  static const uint32_t kUuid32 = 0x12345678;
  const std::vector<uint8_t> kUuid32Bytes = {0x78, 0x56, 0x34, 0x12,
                                             0xab, 0xcd, 0xef};

  static const bluetooth_v2_shlib::Uuid kUuid128 = {
      {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
       0x55, 0x44, 0x00, 0x00}};
  const std::vector<uint8_t> kUuid128Data = {0x78, 0x56, 0x34, 0x12,
                                             0xab, 0xcd, 0xef};
  std::vector<uint8_t> uuid128_bytes(kUuid128.rbegin(), kUuid128.rend());
  uuid128_bytes.insert(uuid128_bytes.end(), kUuid128Data.begin(),
                       kUuid128Data.end());

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapServicesData16bit].push_back(
      kUuid16Bytes);
  scan_result.type_to_data[LeScanResult::kGapServicesData32bit].push_back(
      kUuid32Bytes);
  scan_result.type_to_data[LeScanResult::kGapServicesData128bit].push_back(
      uuid128_bytes);

  auto sd = scan_result.AllServiceData();
  EXPECT_EQ(3ul, sd.size());
  EXPECT_EQ(sd[util::UuidFromInt16(kUuid16)],
            std::vector<uint8_t>(kUuid16Bytes.begin() + sizeof(kUuid16),
                                 kUuid16Bytes.end()));
  EXPECT_EQ(sd[util::UuidFromInt32(kUuid32)],
            std::vector<uint8_t>(kUuid32Bytes.begin() + sizeof(kUuid32),
                                 kUuid32Bytes.end()));
  EXPECT_EQ(sd[kUuid128], kUuid128Data);
}

TEST(LeScanResultTest, ServiceData16Bit) {
  static const uint16_t kUuid1 = 0x1234;
  const std::vector<uint8_t> kUuid1Bytes = {0x34, 0x12, 0xab, 0xcd, 0xef};

  static const uint16_t kUuid2 = 0x5678;
  const std::vector<uint8_t> kUuid2Bytes = {0x78, 0x56, 0xa1, 0x0d, 0xe1};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapServicesData16bit].push_back(
      kUuid1Bytes);
  scan_result.type_to_data[LeScanResult::kGapServicesData16bit].push_back(
      kUuid2Bytes);

  auto sd = scan_result.ServiceData16Bit();
  ASSERT_EQ(2ul, sd.size());
  EXPECT_EQ(sd[util::UuidFromInt16(kUuid1)],
            std::vector<uint8_t>(kUuid1Bytes.begin() + sizeof(kUuid1),
                                 kUuid1Bytes.end()));
  EXPECT_EQ(sd[util::UuidFromInt16(kUuid2)],
            std::vector<uint8_t>(kUuid2Bytes.begin() + sizeof(kUuid2),
                                 kUuid2Bytes.end()));
}

TEST(LeScanResultTest, ServiceData32Bit) {
  static const uint32_t kUuid1 = 0x12345678;
  const std::vector<uint8_t> kUuid1Bytes = {0x78, 0x56, 0x34, 0x12,
                                            0xab, 0xcd, 0xef};

  static const uint32_t kUuid2 = 0xabcdef01;
  const std::vector<uint8_t> kUuid2Bytes = {0x01, 0xef, 0xcd, 0xab,
                                            0xa1, 0x0d, 0xe1};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapServicesData32bit].push_back(
      kUuid1Bytes);
  scan_result.type_to_data[LeScanResult::kGapServicesData32bit].push_back(
      kUuid2Bytes);

  auto sd = scan_result.ServiceData32Bit();
  ASSERT_EQ(2ul, sd.size());
  EXPECT_EQ(sd[util::UuidFromInt32(kUuid1)],
            std::vector<uint8_t>(kUuid1Bytes.begin() + sizeof(kUuid1),
                                 kUuid1Bytes.end()));
  EXPECT_EQ(sd[util::UuidFromInt32(kUuid2)],
            std::vector<uint8_t>(kUuid2Bytes.begin() + sizeof(kUuid2),
                                 kUuid2Bytes.end()));
}

TEST(LeScanResultTest, ServiceData128Bit) {
  static const bluetooth_v2_shlib::Uuid kUuid1 = {
      {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
       0x55, 0x44, 0x00, 0x00}};
  const std::vector<uint8_t> kUuid1Data = {0x78, 0x56, 0x34, 0x12,
                                           0xab, 0xcd, 0xef};
  std::vector<uint8_t> uuid1_bytes(kUuid1.rbegin(), kUuid1.rend());
  uuid1_bytes.insert(uuid1_bytes.end(), kUuid1Data.begin(), kUuid1Data.end());

  static const bluetooth_v2_shlib::Uuid kUuid2 = {
      {0xa8, 0x22, 0xc8, 0x85, 0xaf, 0x02, 0xc7, 0x80, 0x9d, 0x4d, 0xbd, 0x9a,
       0x1f, 0xa0, 0x6d, 0x93}};
  const std::vector<uint8_t> kUuid2Data = {0x01, 0xef, 0xcd, 0xab,
                                           0xa1, 0x0d, 0xe1};
  std::vector<uint8_t> uuid2_bytes(kUuid2.rbegin(), kUuid2.rend());
  uuid2_bytes.insert(uuid2_bytes.end(), kUuid2Data.begin(), kUuid2Data.end());

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapServicesData128bit].push_back(
      uuid1_bytes);
  scan_result.type_to_data[LeScanResult::kGapServicesData128bit].push_back(
      uuid2_bytes);

  auto sd = scan_result.ServiceData128Bit();
  ASSERT_EQ(2ul, sd.size());
  EXPECT_EQ(sd[kUuid1], kUuid1Data);
  EXPECT_EQ(sd[kUuid2], kUuid2Data);
}

TEST(LeScanResultTest, ManufacturerData) {
  static const uint16_t kManufacturer1 = 0x1234;
  const std::vector<uint8_t> kManufacturer1Bytes = {0x34, 0x12, 0xab, 0xcd,
                                                    0xef};

  static const uint16_t kManufacturer2 = 0x5678;
  const std::vector<uint8_t> kManufacturer2Bytes = {0x78, 0x56, 0xa1, 0x0d,
                                                    0xe1};

  LeScanResult scan_result;
  scan_result.type_to_data[LeScanResult::kGapManufacturerData].push_back(
      kManufacturer1Bytes);
  scan_result.type_to_data[LeScanResult::kGapManufacturerData].push_back(
      kManufacturer2Bytes);

  auto md = scan_result.ManufacturerData();
  ASSERT_EQ(2ul, md.size());
  EXPECT_EQ(
      md[kManufacturer1],
      std::vector<uint8_t>(kManufacturer1Bytes.begin() + sizeof(kManufacturer1),
                           kManufacturer1Bytes.end()));
  EXPECT_EQ(
      md[kManufacturer2],
      std::vector<uint8_t>(kManufacturer2Bytes.begin() + sizeof(kManufacturer2),
                           kManufacturer2Bytes.end()));
}

}  // namespace bluetooth
}  // namespace chromecast
