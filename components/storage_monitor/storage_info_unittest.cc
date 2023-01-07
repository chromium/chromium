// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/storage_monitor/storage_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_monitor {

const char kMtpDeviceId[] = "mtp:VendorModelSerial:ABC:1233:1237912873";
const char kUniqueId[] = "VendorModelSerial:ABC:1233:1237912873";
const char kImageCaptureDeviceId[] = "ic:xyz";

// Test to verify |MakeDeviceId| functionality using a sample
// mtp device unique id.
TEST(StorageInfoTest, MakeMtpDeviceId) {
  std::string device_id =
      StorageInfo::MakeDeviceId(StorageInfo::MTP_OR_PTP, kUniqueId);
  ASSERT_EQ(kMtpDeviceId, device_id);
}

// Test to verify |CrackDeviceId| functionality using a sample
// mtp device id.
TEST(StorageInfoTest, CrackMtpDeviceId) {
  StorageInfo::Type type;
  std::string id;
  ASSERT_TRUE(StorageInfo::CrackDeviceId(kMtpDeviceId, &type, &id));
  EXPECT_EQ(kUniqueId, id);
  EXPECT_EQ(StorageInfo::MTP_OR_PTP, type);
}

TEST(StorageInfoTest, TestImageCaptureDeviceId) {
  StorageInfo::Type type;
  std::string id;
  ASSERT_TRUE(StorageInfo::CrackDeviceId(kImageCaptureDeviceId, &type, &id));
  EXPECT_EQ(StorageInfo::MAC_IMAGE_CAPTURE, type);
  EXPECT_EQ("xyz", id);
}

}  // namespace storage_monitor
