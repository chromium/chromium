// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_util.h"

#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using sync_pb::DeviceInfoSpecifics;

namespace syncer {

namespace {

class DeviceInfoUtilTest : public testing::Test {
 protected:
  DeviceInfoUtilTest() {
    // Test cases assume |small_| and |big_| are smaller and bigger,
    // respectively,
    // than both constants.
    EXPECT_LT(small_, DeviceInfoUtil::kActiveThreshold);
    EXPECT_LT(small_, DeviceInfoUtil::kPulseInterval);
    EXPECT_GT(big_, DeviceInfoUtil::kActiveThreshold);
    EXPECT_GT(big_, DeviceInfoUtil::kPulseInterval);
  }

  const Time now_ = Time::Now();
  const TimeDelta small_ = TimeDelta::FromMilliseconds(1);
  const TimeDelta big_ = TimeDelta::FromDays(1000);
};

}  // namespace

TEST_F(DeviceInfoUtilTest, CalculatePulseDelaySame) {
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval,
            DeviceInfoUtil::CalculatePulseDelay(Time(), Time()));
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval,
            DeviceInfoUtil::CalculatePulseDelay(now_, now_));
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval,
            DeviceInfoUtil::CalculatePulseDelay(now_ + big_, now_ + big_));
}

TEST_F(DeviceInfoUtilTest, CalculatePulseDelayMiddle) {
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval - small_,
            DeviceInfoUtil::CalculatePulseDelay(Time(), Time() + small_));
  EXPECT_EQ(small_,
            DeviceInfoUtil::CalculatePulseDelay(
                Time(), Time() + DeviceInfoUtil::kPulseInterval - small_));
}

TEST_F(DeviceInfoUtilTest, CalculatePulseDelayStale) {
  EXPECT_EQ(TimeDelta(), DeviceInfoUtil::CalculatePulseDelay(
                             Time(), Time() + DeviceInfoUtil::kPulseInterval));
  EXPECT_EQ(TimeDelta(),
            DeviceInfoUtil::CalculatePulseDelay(
                Time(), Time() + DeviceInfoUtil::kPulseInterval + small_));
  EXPECT_EQ(TimeDelta(),
            DeviceInfoUtil::CalculatePulseDelay(
                Time(), Time() + DeviceInfoUtil::kPulseInterval + small_));
  EXPECT_EQ(TimeDelta(), DeviceInfoUtil::CalculatePulseDelay(
                             now_, now_ + DeviceInfoUtil::kPulseInterval));
}

TEST_F(DeviceInfoUtilTest, CalculatePulseDelayFuture) {
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval,
            DeviceInfoUtil::CalculatePulseDelay(Time() + small_, Time()));
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval,
            DeviceInfoUtil::CalculatePulseDelay(
                Time() + DeviceInfoUtil::kPulseInterval, Time()));
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval,
            DeviceInfoUtil::CalculatePulseDelay(Time() + big_, Time()));
  EXPECT_EQ(DeviceInfoUtil::kPulseInterval,
            DeviceInfoUtil::CalculatePulseDelay(now_ + big_, now_));
}

TEST_F(DeviceInfoUtilTest, IsActive) {
  EXPECT_TRUE(DeviceInfoUtil::IsActive(Time(), Time()));
  EXPECT_TRUE(DeviceInfoUtil::IsActive(now_, now_));
  EXPECT_TRUE(DeviceInfoUtil::IsActive(now_, now_ + small_));
  EXPECT_TRUE(DeviceInfoUtil::IsActive(
      now_, now_ + DeviceInfoUtil::kActiveThreshold - small_));
  EXPECT_FALSE(
      DeviceInfoUtil::IsActive(now_, now_ + DeviceInfoUtil::kActiveThreshold));
  EXPECT_FALSE(DeviceInfoUtil::IsActive(
      now_, now_ + DeviceInfoUtil::kActiveThreshold + small_));
  EXPECT_FALSE(DeviceInfoUtil::IsActive(
      now_, now_ + DeviceInfoUtil::kActiveThreshold + big_));
  EXPECT_TRUE(DeviceInfoUtil::IsActive(now_ + small_, now_));
  EXPECT_TRUE(DeviceInfoUtil::IsActive(now_ + big_, now_));
}

TEST_F(DeviceInfoUtilTest, TagRoundTrip) {
  DeviceInfoSpecifics specifics;
  ASSERT_EQ("", DeviceInfoUtil::TagToCacheGuid(
                    DeviceInfoUtil::SpecificsToTag(specifics)));

  std::string cache_guid("guid");
  specifics.set_cache_guid(cache_guid);
  ASSERT_EQ(cache_guid, DeviceInfoUtil::TagToCacheGuid(
                            DeviceInfoUtil::SpecificsToTag(specifics)));

  specifics.set_cache_guid(DeviceInfoUtil::kClientTagPrefix);
  ASSERT_EQ(DeviceInfoUtil::kClientTagPrefix,
            DeviceInfoUtil::TagToCacheGuid(
                DeviceInfoUtil::SpecificsToTag(specifics)));
}

}  // namespace syncer
