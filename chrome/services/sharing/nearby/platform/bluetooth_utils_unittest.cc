// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

TEST(BluetoothUtilsTest, UuidConversionSuccessful) {
  char uuid[] = "12345678-1234-5678-9abc-def123456789";
  device::BluetoothUUID bt_uuid(uuid);
  EXPECT_TRUE(bt_uuid.IsValid());

  Uuid nearby_uuid = BluetoothUuidToNearbyUuid(bt_uuid);
  EXPECT_EQ(bt_uuid, device::BluetoothUUID(std::string(nearby_uuid)));
}

}  // namespace nearby::chrome
