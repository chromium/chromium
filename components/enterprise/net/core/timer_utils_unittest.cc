// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/timer_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

TEST(TimerUtilsTest, CalculateProactiveRefreshDelay_DefaultExpiration) {
  base::Time now = base::Time::Now();
  // With null expiration, default 1 hour TTL is used. 80% of 1 hour is 48
  // minutes.
  base::TimeDelta delay =
      CalculateProactiveRefreshDelay(/*expires=*/base::Time(), now);
  EXPECT_EQ(base::Minutes(48), delay);
}

TEST(TimerUtilsTest, CalculateProactiveRefreshDelay_StandardTtl) {
  base::Time now = base::Time::Now();
  base::Time expires = now + base::Hours(10);
  // 80% of 10 hours is 8 hours.
  base::TimeDelta delay = CalculateProactiveRefreshDelay(expires, now);
  EXPECT_EQ(base::Hours(8), delay);
}

TEST(TimerUtilsTest, CalculateProactiveRefreshDelay_EnforcesMinFloor) {
  base::Time now = base::Time::Now();
  // 80% of 30 seconds is 24 seconds, which is below the 1 minute minimum floor.
  base::Time expires = now + base::Seconds(30);
  base::TimeDelta delay = CalculateProactiveRefreshDelay(expires, now);
  EXPECT_EQ(kMinRefreshDelay, delay);
}

TEST(TimerUtilsTest, CalculateProactiveRefreshDelay_PastExpiration) {
  base::Time now = base::Time::Now();
  // Expired in the past.
  base::Time expires = now - base::Minutes(5);
  base::TimeDelta delay = CalculateProactiveRefreshDelay(expires, now);
  EXPECT_EQ(kMinRefreshDelay, delay);
}

TEST(TimerUtilsTest, CalculateTransientRetryDelay_ExponentialBackoff) {
  // 1st failure: 15s
  EXPECT_EQ(base::Seconds(15), CalculateTransientRetryDelay(1));
  EXPECT_EQ(base::Seconds(15), CalculateTransientRetryDelay(0));

  // 2nd failure: 15s * 4 = 60s (1m)
  EXPECT_EQ(base::Minutes(1), CalculateTransientRetryDelay(2));

  // 3rd failure: 15s * 16 = 240s (4m)
  EXPECT_EQ(base::Minutes(4), CalculateTransientRetryDelay(3));

  // 4th+ failures: capped at 16m
  EXPECT_EQ(base::Minutes(16), CalculateTransientRetryDelay(4));
  EXPECT_EQ(base::Minutes(16), CalculateTransientRetryDelay(5));
  EXPECT_EQ(base::Minutes(16), CalculateTransientRetryDelay(10));
}

}  // namespace enterprise_net
