// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"

#include <stdint.h>

#include <string>

#include "base/time/time.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

const char kFutureTime[] = "31 Dec 2020 23:59:59.001";

}  // namespace

class DataReductionProxyClientProtobufParserTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(base::Time::FromUTCString(kFutureTime, &future_time_));
  }

  const base::Time& GetFutureTime() const { return future_time_; }

 private:
  base::Time future_time_;
};

TEST_F(DataReductionProxyClientProtobufParserTest, TimeDeltaToFromDuration) {
  const struct {
    std::string test_name;
    base::TimeDelta time_delta;
    int64_t seconds;
    int32_t nanos;
  } tests[] = {
      {
          "Second", base::TimeDelta::FromSeconds(1), 1, 0,
      },
      {
          "-1 Second", base::TimeDelta::FromSeconds(-1), -1, 0,
      },
      {
          "1.5 Seconds", base::TimeDelta::FromMilliseconds(1500), 1,
          base::Time::kNanosecondsPerSecond / 2,
      },
  };

  for (const auto& test : tests) {
    Duration duration;
    protobuf_parser::TimeDeltaToDuration(test.time_delta, &duration);
    EXPECT_EQ(test.seconds, duration.seconds()) << test.test_name;
    EXPECT_EQ(test.nanos, duration.nanos()) << test.test_name;
    duration.set_seconds(test.seconds);
    duration.set_nanos(test.nanos);
    EXPECT_EQ(test.time_delta, protobuf_parser::DurationToTimeDelta(duration))
        << test.test_name;
  }
}

TEST_F(DataReductionProxyClientProtobufParserTest, TimeStampToFromTime) {
  const struct {
    std::string test_name;
    base::Time time;
    int64_t seconds;
    int32_t nanos;
  } tests[] = {
      {
          "Second", base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1),
          1, 0,
      },
      {
          "1.5 Seconds",
          base::Time::UnixEpoch() + base::TimeDelta::FromMilliseconds(1500), 1,
          base::Time::kNanosecondsPerSecond / 2,
      },
  };

  for (const auto& test : tests) {
    Timestamp timestamp;
    protobuf_parser::TimeToTimestamp(test.time, &timestamp);
    EXPECT_EQ(test.seconds, timestamp.seconds()) << test.test_name;
    EXPECT_EQ(test.nanos, timestamp.nanos()) << test.test_name;
    timestamp.set_seconds(test.seconds);
    timestamp.set_nanos(test.nanos);
    EXPECT_EQ(test.time, protobuf_parser::TimestampToTime(timestamp))
        << test.test_name;
  }
}

}  // namespace data_reduction_proxy
