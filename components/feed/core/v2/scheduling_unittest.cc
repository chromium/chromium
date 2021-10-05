// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/scheduling.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const base::Time kAnchorTime = base::Time::UnixEpoch() + base::Hours(6);
const base::TimeDelta kDefaultScheduleInterval = base::Hours(24);

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
  schedule.refresh_offsets = {base::Hours(1), base::Hours(6)};

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
  schedule.refresh_offsets = {base::Hours(1), base::Hours(6)};

  // |kNow| is in the normal range [kAnchorTime, kAnchorTime+1hr)
  base::Time kNow = kAnchorTime + base::Minutes(12);
  EXPECT_EQ(kAnchorTime + base::Hours(1),
            NextScheduledRequestTime(kNow, &schedule));
  kNow += base::Hours(1);
  EXPECT_EQ(kAnchorTime + base::Hours(6),
            NextScheduledRequestTime(kNow, &schedule));
  kNow += base::Hours(6);
  EXPECT_EQ(kNow + kDefaultScheduleInterval,
            NextScheduledRequestTime(kNow, &schedule));
}

TEST_F(NextScheduledRequestTimeTest, NowPastRequestTimeSkipsRequest) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {base::Hours(1), base::Hours(6)};

  base::Time kNow = kAnchorTime + base::Minutes(61);
  EXPECT_EQ(kAnchorTime + base::Hours(6),
            NextScheduledRequestTime(kNow, &schedule));
  kNow += base::Hours(6);
  EXPECT_EQ(kNow + kDefaultScheduleInterval,
            NextScheduledRequestTime(kNow, &schedule));
}

TEST_F(NextScheduledRequestTimeTest, NowPastAllRequestTimes) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {base::Hours(1), base::Hours(6)};

  base::Time kNow = kAnchorTime + base::Hours(7);
  EXPECT_EQ(kNow + kDefaultScheduleInterval,
            NextScheduledRequestTime(kNow, &schedule));
}

TEST_F(NextScheduledRequestTimeTest, NowInPast) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {base::Hours(1), base::Hours(6)};

  // Since |kNow| is in the past, deltas are recomputed using |kNow|.
  base::Time kNow = kAnchorTime - base::Minutes(12);
  EXPECT_EQ(kNow + base::Hours(1), NextScheduledRequestTime(kNow, &schedule));
  EXPECT_EQ(kNow, schedule.anchor_time);
}

TEST_F(NextScheduledRequestTimeTest, NowInFarFuture) {
  RequestSchedule schedule;
  schedule.anchor_time = kAnchorTime;
  schedule.refresh_offsets = {base::Hours(1), base::Hours(6)};

  // Since |kNow| is in the far future, deltas are recomputed using |kNow|.
  base::Time kNow = kAnchorTime + base::Days(12);
  EXPECT_EQ(kNow + base::Hours(1), NextScheduledRequestTime(kNow, &schedule));
  EXPECT_EQ(kNow, schedule.anchor_time);
}

class ContentLifetimeTest : public testing::Test {
 public:
  const base::TimeDelta kDefaultContentExpiration = base::Hours(24);
  const base::TimeDelta kDefaultStaleContentThreshold = base::Hours(4);

  void SetUp() override {
    Config config = GetFeedConfig();
    config.content_expiration_threshold = kDefaultContentExpiration;
    config.stale_content_threshold = kDefaultStaleContentThreshold;
    SetFeedConfigForTesting(config);

    metadata_ = feedstore::Metadata();
    feedstore::Metadata::StreamMetadata* sm = metadata_.add_stream_metadata();
    sm->set_stream_id(std::string(feedstore::StreamId(kForYouStream)));
  }

 protected:
  void set_content_lifetime(int64_t stale_age_ms, int64_t invalid_age_ms) {
    feedstore::Metadata::StreamMetadata::ContentLifetime* content_lifetime =
        MetadataForStream(metadata_, kForYouStream).mutable_content_lifetime();
    content_lifetime->set_stale_age_ms(stale_age_ms);
    content_lifetime->set_invalid_age_ms(invalid_age_ms);
  }

  void set_stale_age(base::TimeDelta stale_age) {
    set_content_lifetime(stale_age.InMilliseconds(), 0);
  }

  void set_invalid_age(base::TimeDelta invalid_age) {
    set_content_lifetime(0, invalid_age.InMilliseconds());
  }

  base::TimeDelta WithEpsilon(base::TimeDelta duration) {
    return duration + base::Milliseconds(1);
  }

  feedstore::Metadata metadata_;
};

TEST_F(ContentLifetimeTest, ShouldWaitForNewContent_DefaultThreshold) {
  EXPECT_FALSE(ShouldWaitForNewContent(metadata_, kForYouStream,
                                       kDefaultStaleContentThreshold));
  EXPECT_TRUE(ShouldWaitForNewContent(
      metadata_, kForYouStream, WithEpsilon(kDefaultStaleContentThreshold)));
  EXPECT_TRUE(
      ShouldWaitForNewContent(metadata_, kForYouStream, base::Hours(5)));
  EXPECT_FALSE(
      ShouldWaitForNewContent(metadata_, kForYouStream, base::Hours(3)));
}

TEST_F(ContentLifetimeTest, ShouldWaitForNewContent_ServerThreshold_Valid) {
  set_stale_age(base::Minutes(60));
  EXPECT_TRUE(
      ShouldWaitForNewContent(metadata_, kForYouStream, base::Minutes(61)));
  EXPECT_FALSE(
      ShouldWaitForNewContent(metadata_, kForYouStream, base::Minutes(59)));
}

TEST_F(ContentLifetimeTest, ShouldWaitForNewContent_ServerThreshold_Invalid) {
  // We ignore stale ages greater than the default.
  EXPECT_TRUE(ShouldWaitForNewContent(
      metadata_, kForYouStream, WithEpsilon(kDefaultStaleContentThreshold)));
  set_stale_age(kDefaultStaleContentThreshold + base::Minutes(1));
  EXPECT_TRUE(ShouldWaitForNewContent(
      metadata_, kForYouStream, WithEpsilon(kDefaultStaleContentThreshold)));

  // We ignore zero durations.
  set_stale_age(base::Days(0));
  EXPECT_FALSE(ShouldWaitForNewContent(metadata_, kForYouStream,
                                       kDefaultStaleContentThreshold));
  EXPECT_TRUE(ShouldWaitForNewContent(
      metadata_, kForYouStream, WithEpsilon(kDefaultStaleContentThreshold)));

  // We ignore negative durations.
  set_stale_age(base::Days(-1));
  EXPECT_FALSE(ShouldWaitForNewContent(metadata_, kForYouStream,
                                       kDefaultStaleContentThreshold));
  EXPECT_TRUE(ShouldWaitForNewContent(
      metadata_, kForYouStream, WithEpsilon(kDefaultStaleContentThreshold)));
}

TEST_F(ContentLifetimeTest, ContentInvalidFromAge_DefaultThreshold) {
  EXPECT_FALSE(ContentInvalidFromAge(metadata_, kForYouStream,
                                     kDefaultContentExpiration));
  EXPECT_TRUE(
      ContentInvalidFromAge(metadata_, kForYouStream,
                            kDefaultContentExpiration + base::Milliseconds(1)));
  EXPECT_TRUE(ContentInvalidFromAge(metadata_, kForYouStream, base::Hours(25)));
  EXPECT_FALSE(
      ContentInvalidFromAge(metadata_, kForYouStream, base::Hours(23)));
}

TEST_F(ContentLifetimeTest, ContentInvalidFromAge_ServerThreshold_Valid) {
  set_invalid_age(base::Minutes(60));
  EXPECT_TRUE(
      ContentInvalidFromAge(metadata_, kForYouStream, base::Minutes(61)));
  EXPECT_FALSE(
      ContentInvalidFromAge(metadata_, kForYouStream, base::Minutes(59)));
}

TEST_F(ContentLifetimeTest, ContentInvalidFromAge_ServerThreshold_Invalid) {
  // We ignore stale ages greater than the default.
  EXPECT_TRUE(ContentInvalidFromAge(metadata_, kForYouStream,
                                    WithEpsilon(kDefaultContentExpiration)));
  set_invalid_age(kDefaultContentExpiration + base::Minutes(1));
  EXPECT_TRUE(ContentInvalidFromAge(metadata_, kForYouStream,
                                    WithEpsilon(kDefaultContentExpiration)));

  // We ignore zero durations.
  set_invalid_age(base::Days(0));
  EXPECT_FALSE(ContentInvalidFromAge(metadata_, kForYouStream,
                                     kDefaultContentExpiration));
  EXPECT_TRUE(ContentInvalidFromAge(metadata_, kForYouStream,
                                    WithEpsilon(kDefaultContentExpiration)));

  // We ignore negative durations.
  set_invalid_age(base::Days(-1));
  EXPECT_FALSE(ContentInvalidFromAge(metadata_, kForYouStream,
                                     kDefaultContentExpiration));
  EXPECT_TRUE(ContentInvalidFromAge(metadata_, kForYouStream,
                                    WithEpsilon(kDefaultContentExpiration)));
}

}  // namespace
}  // namespace feed
