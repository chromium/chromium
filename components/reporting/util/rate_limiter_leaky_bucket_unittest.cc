// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/rate_limiter_leaky_bucket.h"

#include <cstddef>

#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

constexpr size_t kMaxLevel = 1024u;
constexpr base::TimeDelta kFillingTime = base::Seconds(16);
constexpr base::TimeDelta kFillingPeriod = base::Seconds(2);
constexpr size_t kEventCount = 8;

class RateLimiterLeakyBucketTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  RateLimiterLeakyBucket rate_limiter_{kMaxLevel, kFillingTime, kFillingPeriod};
};

TEST_F(RateLimiterLeakyBucketTest, SingularEvent) {
  // Initially - not even 1-byte event can be accepted.
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
  // Same when bucket is almost full.
  task_environment_.FastForwardBy(kFillingTime - kFillingPeriod);
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
  // Accept only once it is full.
  task_environment_.FastForwardBy(kFillingPeriod);
  ASSERT_TRUE(rate_limiter_.Acquire(kMaxLevel));
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
}

TEST_F(RateLimiterLeakyBucketTest, SteadyEventsStream) {
  // Let the bucket fill in.
  task_environment_.FastForwardBy(kFillingTime);
  // Drop one event every `kFillingPeriod` sec,
  // allowing one event through and no more.
  for (size_t i = 0; i < kEventCount; ++i) {
    ASSERT_TRUE(rate_limiter_.Acquire(1u));
    ASSERT_FALSE(rate_limiter_.Acquire(1u));
    // We only used 1 byte, will fill in again in 1 period.
    task_environment_.FastForwardBy(kFillingPeriod);
  }
}

TEST_F(RateLimiterLeakyBucketTest, RandomizedEventsStream) {
  // Let the bucket fill in.
  task_environment_.FastForwardBy(kFillingTime);
  // Drop one event every `kFillingPeriod + random` sec,
  // allowing one event through and no more.
  for (size_t i = 0; i < kEventCount; ++i) {
    ASSERT_TRUE(rate_limiter_.Acquire(1u));
    ASSERT_FALSE(rate_limiter_.Acquire(1u));
    // We only used 1 byte, will fill in again in 1 period, plus add a random.
    task_environment_.FastForwardBy(kFillingPeriod +
                                    base::Milliseconds(base::RandInt(0, 100)));
  }
}

TEST_F(RateLimiterLeakyBucketTest, LargeEventsStream) {
  // Let the bucket fill in.
  task_environment_.FastForwardBy(kFillingTime);
  // Drop one event every `kFillingTime` sec,
  // allowing one event through and no more.
  for (size_t i = 0; i < kEventCount; ++i) {
    // See that it was enough for maximum event size, but no more.
    ASSERT_TRUE(rate_limiter_.Acquire(kMaxLevel));
    ASSERT_FALSE(rate_limiter_.Acquire(1u));
    // `kFillingPeriod` is not sufficient now.
    task_environment_.FastForwardBy(kFillingPeriod);
    ASSERT_FALSE(rate_limiter_.Acquire(1u));
    // Let the bucket fill in.
    task_environment_.FastForwardBy(kFillingTime - kFillingPeriod);
  }
}
}  // namespace
}  // namespace reporting
