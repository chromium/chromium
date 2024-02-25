// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/native_metrics_util.h"

#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

namespace native_metrics_util {

namespace {

TEST(NativeMetricsUtilTest, ConvertsTimes) {
  constexpr auto start_delta = base::Milliseconds(20);
  constexpr auto event_delta = base::Milliseconds(30);

  std::optional<Cronet_DateTime> converted;
  ConvertTime(base::TimeTicks::UnixEpoch() + event_delta,
              base::TimeTicks::UnixEpoch() + start_delta,
              base::Time::UnixEpoch() + start_delta, &converted);
  ASSERT_TRUE(converted.has_value());
  EXPECT_EQ(converted->value, 30);
}

TEST(NativeMetricsUtilTest, OverwritesOldOutParam) {
  constexpr auto start_delta = base::Milliseconds(20);
  constexpr auto event_delta = base::Milliseconds(30);

  std::optional<Cronet_DateTime> converted;
  converted.emplace();
  converted->value = 60;
  ConvertTime(base::TimeTicks::UnixEpoch() + event_delta,
              base::TimeTicks::UnixEpoch() + start_delta,
              base::Time::UnixEpoch() + start_delta, &converted);
  ASSERT_TRUE(converted.has_value());
  EXPECT_EQ(converted->value, 30);
}

TEST(NativeMetricsUtilTest, NullTicks) {
  constexpr auto start_delta = base::Milliseconds(20);

  std::optional<Cronet_DateTime> converted;
  ConvertTime(base::TimeTicks(), base::TimeTicks::UnixEpoch() + start_delta,
              base::Time::UnixEpoch() + start_delta, &converted);
  ASSERT_FALSE(converted.has_value());
}

TEST(NativeMetricsUtilTest, NullStartTicks) {
  constexpr auto start_delta = base::Milliseconds(20);
  constexpr auto event_delta = base::Milliseconds(30);

  std::optional<Cronet_DateTime> converted;
  ConvertTime(base::TimeTicks::UnixEpoch() + event_delta, base::TimeTicks(),
              base::Time::UnixEpoch() + start_delta, &converted);
  ASSERT_FALSE(converted.has_value());
}

TEST(NativeMetricsUtilTest, NullStartTime) {
  constexpr auto start_delta = base::Milliseconds(20);
  constexpr auto event_delta = base::Milliseconds(30);

  std::optional<Cronet_DateTime> converted;
  EXPECT_DCHECK_DEATH(ConvertTime(base::TimeTicks::UnixEpoch() + event_delta,
                                  base::TimeTicks::UnixEpoch() + start_delta,
                                  base::Time(), &converted));
}

}  // namespace

}  // namespace native_metrics_util

}  // namespace cronet
