// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/proto_conversions.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kUserEmail = "testtester@gmail.com";
const std::string kDeviceName = "Test's Chromebook";
const std::string kUserName = "Test Tester";
const std::string kProfileUrl = "https://example.com";
const std::string kMacAddress = "1A:2B:3C:4D:5E:6F";

}  // namespace

namespace ash::nearby::presence {

class ProtoConversionsTest : public testing::Test {};

TEST_F(ProtoConversionsTest, BuildMetadata) {
  ::nearby::internal::Metadata metadata = BuildMetadata(
      /*device_type=*/::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP,
      /*account_name=*/kUserEmail,
      /*device_name=*/kDeviceName,
      /*user_name=*/kUserName,
      /*profile_url=*/kProfileUrl,
      /*mac_address=*/kMacAddress);

  EXPECT_EQ(kUserEmail, metadata.account_name());
  EXPECT_EQ(kDeviceName, metadata.device_name());
  EXPECT_EQ(kUserName, metadata.user_name());
  EXPECT_EQ(kProfileUrl, metadata.device_profile_url());
  EXPECT_EQ(kMacAddress, metadata.bluetooth_mac_address());
}

}  // namespace ash::nearby::presence
