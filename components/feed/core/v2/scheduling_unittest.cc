// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/scheduling.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/feed/core/v2/config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using base::TimeDelta;

const base::Time kAnchorTime =
    base::Time::UnixEpoch() + TimeDelta::FromHours(6);
const base::TimeDelta kDefaultScheduleInterval = base::TimeDelta::FromHours(24);

std::string ToJSON(const base::Value& value) {
  std::string json;
  CHECK(base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json));
  // Don't use \r\n on windows.
  base::RemoveChars(json, "\r", &json);
  return json;
}

TEST(RequestSchedule, CanSerialize) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {TimeDelta::FromHours(1), TimeDelta::FromHours(6)};

  const base::Value schedule_value = RequestScheduleToValue(schedule);
  ASSERT_EQ(R"({
   "anchor": "11644495200000000",
   "offsets": [ "3600000000", "21600000000" ]
}
)",
            ToJSON(schedule_value));

  RequestSchedule deserialized_schedule =
      RequestScheduleFromValue(schedule_value);
  EXPECT_EQ(schedule.anchor_time, deserialized_schedule.anchor_time);
  EXPECT_EQ(schedule.refresh_offsets, deserialized_schedule.refresh_offsets);
}

class NextScheduledRequestTimeTest : public testing::Test {
 public:
  void SetUp() override {
    Config config = GetFeedConfig();
    config.default_background_refresh_interval = kDefaultScheduleInterval;
    SetFeedConfigForTesting(config);
  }
};

TEST_F(NextScheduledRequestTimeTest, NormalUsage) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {TimeDelta::FromHours(1), TimeDelta::FromHours(6)};

  // |kNow| is in the normal range [kAnchorTime, kAnchorTime+1hr)
  base::Time kNow = kAnchorTime + TimeDelta::FromMinutes(12);
  EXPECT_EQ(kAnchorTime + TimeDelta::FromHours(1),
            NextScheduledRequestTime(kNow, &schedule));
  kNow += TimeDelta::FromHours(1);
  EXPECT_EQ(kAnchorTime + TimeDelta::FromHours(6),
            NextScheduledRequestTime(kNow, &schedule));
  kNow += TimeDelta::FromHours(6);
  EXPECT_EQ(kNow + kDefaultScheduleInterval,
            NextScheduledRequestTime(kNow, &schedule));
}

TEST_F(NextScheduledRequestTimeTest, NowPastRequestTimeSkipsRequest) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {TimeDelta::FromHours(1), TimeDelta::FromHours(6)};

  base::Time kNow = kAnchorTime + TimeDelta::FromMinutes(61);
  EXPECT_EQ(kAnchorTime + TimeDelta::FromHours(6),
            NextScheduledRequestTime(kNow, &schedule));
  kNow += TimeDelta::FromHours(6);
  EXPECT_EQ(kNow + kDefaultScheduleInterval,
            NextScheduledRequestTime(kNow, &schedule));
}

TEST_F(NextScheduledRequestTimeTest, NowPastAllRequestTimes) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {TimeDelta::FromHours(1), TimeDelta::FromHours(6)};

  base::Time kNow = kAnchorTime + TimeDelta::FromHours(7);
  EXPECT_EQ(kNow + kDefaultScheduleInterval,
            NextScheduledRequestTime(kNow, &schedule));
}

TEST_F(NextScheduledRequestTimeTest, NowInPast) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {TimeDelta::FromHours(1), TimeDelta::FromHours(6)};

  // Since |kNow| is in the past, deltas are recomputed using |kNow|.
  base::Time kNow = kAnchorTime - TimeDelta::FromMinutes(12);
  EXPECT_EQ(kNow + TimeDelta::FromHours(1),
            NextScheduledRequestTime(kNow, &schedule));
  EXPECT_EQ(kNow, schedule.anchor_time);
}

TEST_F(NextScheduledRequestTimeTest, NowInFarFuture) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {TimeDelta::FromHours(1), TimeDelta::FromHours(6)};

  // Since |kNow| is in the far future, deltas are recomputed using |kNow|.
  base::Time kNow = kAnchorTime + TimeDelta::FromDays(12);
  EXPECT_EQ(kNow + TimeDelta::FromHours(1),
            NextScheduledRequestTime(kNow, &schedule));
  EXPECT_EQ(kNow, schedule.anchor_time);
}

}  // namespace
}  // namespace feed
