// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/rate_limiter_slide_window.h"

#include <cstddef>

#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

constexpr size_t kTotalSize = 1024u;
constexpr base::TimeDelta kTimeWindow = base::Seconds(16);
constexpr size_t kBucketCount = 8;
constexpr base::TimeDelta kBucket = kTimeWindow / kBucketCount;

class RateLimiterSlideWindowTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  RateLimiterSlideWindow rate_limiter_{kTotalSize, kTimeWindow, kBucketCount};
};

TEST_F(RateLimiterSlideWindowTest, SingularEvent) {
  ASSERT_FALSE(rate_limiter_.Acquire(kTotalSize + 1));
  ASSERT_TRUE(rate_limiter_.Acquire(kTotalSize));
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
  task_environment_.FastForwardBy(kTimeWindow - kBucket);
  ASSERT_FALSE(rate_limiter_.Acquire(kTotalSize));
  task_environment_.FastForwardBy(kBucket);
  ASSERT_TRUE(rate_limiter_.Acquire(kTotalSize));
  ASSERT_FALSE(rate_limiter_.Acquire(1u));
}

TEST_F(RateLimiterSlideWindowTest, SteadyEventsStream) {
  for (size_t i = 0; i < 2 * kBucketCount; ++i) {
    ASSERT_TRUE(rate_limiter_.Acquire(kTotalSize / kBucketCount));
    if (i >= kBucketCount) {
      ASSERT_FALSE(rate_limiter_.Acquire(1u));
    }
    task_environment_.FastForwardBy(kBucket);
  }
}

TEST_F(RateLimiterSlideWindowTest, RandomizedEventsStream) {
  for (size_t i = 0; i < 2 * kBucketCount; ++i) {
    ASSERT_TRUE(rate_limiter_.Acquire(kTotalSize / kBucketCount));
    if (i >= kBucketCount) {
      ASSERT_FALSE(rate_limiter_.Acquire(1u));
    }
    task_environment_.FastForwardBy(kBucket +
                                    base::Milliseconds(base::RandInt(0, 100)));
  }
}

TEST_F(RateLimiterSlideWindowTest, SparseEventsStream) {
  for (size_t i = 0; i < 2 * kBucketCount; ++i) {
    ASSERT_TRUE(rate_limiter_.Acquire(1u));
    task_environment_.FastForwardBy(kTimeWindow - base::Milliseconds(1));
  }
}
}  // namespace
}  // namespace reporting
