// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/device_metadata_utils.h"

#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::report::utils {

class DeviceMetadataUtilsTest : public testing::Test {
 public:
  DeviceMetadataUtilsTest() = default;
  DeviceMetadataUtilsTest(const DeviceMetadataUtilsTest&) = delete;
  DeviceMetadataUtilsTest& operator=(const DeviceMetadataUtilsTest&) = delete;
  ~DeviceMetadataUtilsTest() override = default;

  void SetUp() override {
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);
  }

  void TearDown() override {
    system::StatisticsProvider::SetTestProvider(nullptr);
  }

 protected:
  system::FakeStatisticsProvider& GetStatisticsProvider() {
    return statistics_provider_;
  }

 private:
  system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(DeviceMetadataUtilsTest, GetChromeChannel) {
  EXPECT_EQ(GetChromeChannel(version_info::Channel::CANARY),
            Channel::CHANNEL_CANARY);
  EXPECT_EQ(GetChromeChannel(version_info::Channel::DEV), Channel::CHANNEL_DEV);
  EXPECT_EQ(GetChromeChannel(version_info::Channel::BETA),
            Channel::CHANNEL_BETA);
  EXPECT_EQ(GetChromeChannel(version_info::Channel::STABLE),
            Channel::CHANNEL_STABLE);
  EXPECT_EQ(GetChromeChannel(version_info::Channel::UNKNOWN),
            Channel::CHANNEL_UNKNOWN);
}

TEST_F(DeviceMetadataUtilsTest, GetChromeMilestone) {
  std::string milestone = GetChromeMilestone();
  EXPECT_FALSE(milestone.empty());
  // Perform additional tests based on your expectations for the milestone
  // value. For example, check if it's a valid version number format.
}

TEST_F(DeviceMetadataUtilsTest, GetFullHardwareClass) {
  EXPECT_EQ(GetFullHardwareClass(), "HARDWARE_CLASS_KEY_NOT_FOUND");

  std::string real_hardware_class =
      "PHASER360 K6H-B44-I4F-B2E-R5M-R4B-A5I-A2A-Q3O";
  GetStatisticsProvider().SetMachineStatistic(system::kHardwareClassKey,
                                              real_hardware_class);

  EXPECT_EQ(GetFullHardwareClass(), real_hardware_class);
}

}  // namespace ash::report::utils
