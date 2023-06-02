// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/rate_limiter_token_bucket.h"

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

class RateLimiterTokenBucketTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  RateLimiterTokenBucket rate_limiter_{kMaxLevel, kFillingTime, kFillingPeriod};
};

TEST_F(RateLimiterTokenBucketTest, SingularEvent) {
  // Initially - not even 1-byte event can be accepted.
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
  // Accept only once enough tokens have been added.
  task_environment_.FastForwardBy(kFillingPeriod);
  ASSERT_TRUE(rate_limiter_.Acquire(kMaxLevel * kFillingPeriod / kFillingTime));
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
  // Same when bucket is full.
  task_environment_.FastForwardBy(kFillingTime);
  ASSERT_TRUE(rate_limiter_.Acquire(kMaxLevel));
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
}

TEST_F(RateLimiterTokenBucketTest, SteadyEventsStream) {
  // Drop one event every `kFillingPeriod` sec,
  // allowing one event through and no more.
  for (size_t i = 0; i < kEventCount; ++i) {
    // Let the minimal intake (step forward by 1 period).
    task_environment_.FastForwardBy(kFillingPeriod);
    // See that it was enough for matching event size, but no more.
    ASSERT_TRUE(
        rate_limiter_.Acquire(kMaxLevel * kFillingPeriod / kFillingTime));
    ASSERT_FALSE(rate_limiter_.Acquire(1u));
  }
}

TEST_F(RateLimiterTokenBucketTest, RandomizedEventsStream) {
  // Drop one event every `kFillingPeriod + random` sec,
  // allowing one event through and no more.
  for (size_t i = 0; i < kEventCount; ++i) {
    // Allow the minimal intake (step forward by 1 period, plus add a random).
    task_environment_.FastForwardBy(kFillingPeriod +
                                    base::Milliseconds(base::RandInt(0, 100)));
    // See that it was enough for matching event size, but no more.
    ASSERT_TRUE(
        rate_limiter_.Acquire(kMaxLevel * kFillingPeriod / kFillingTime));
    ASSERT_FALSE(rate_limiter_.Acquire(1u));
  }
}

TEST_F(RateLimiterTokenBucketTest, LargeEventsStream) {
  // Drop one event every `kFillingTime` sec,
  // allowing one event through and no more.
  for (size_t i = 0; i < kEventCount; ++i) {
    // `kFillingTime - kFillingPeriod` is not sufficient.
    task_environment_.FastForwardBy(kFillingTime - kFillingPeriod);
    ASSERT_FALSE(rate_limiter_.Acquire(kMaxLevel));
    // Let the bucket fill in.
    // See that it was enough for maximum event size, but no more.
    task_environment_.FastForwardBy(kFillingPeriod);
    ASSERT_TRUE(rate_limiter_.Acquire(kMaxLevel));
    ASSERT_FALSE(rate_limiter_.Acquire(1u));
  }
}
}  // namespace
}  // namespace reporting
