// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/rate_estimator.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

TEST(RateEstimatorTest, RateEstimator) {
  base::TimeTicks now;
  RateEstimator estimator(base::Seconds(1), 10u, now);
  EXPECT_EQ(0u, estimator.GetCountPerSecond(now));

  estimator.Increment(50u, now);
  EXPECT_EQ(50u, estimator.GetCountPerSecond(now));

  now += base::Milliseconds(800);
  estimator.Increment(50, now);
  EXPECT_EQ(100u, estimator.GetCountPerSecond(now));

  // Advance time.
  now += base::Seconds(3);
  EXPECT_EQ(25u, estimator.GetCountPerSecond(now));
  estimator.Increment(60, now);
  EXPECT_EQ(40u, estimator.GetCountPerSecond(now));

  // Advance time again.
  now += base::Seconds(4);
  EXPECT_EQ(20u, estimator.GetCountPerSecond(now));

  // Advance time to the end.
  now += base::Seconds(2);
  EXPECT_EQ(16u, estimator.GetCountPerSecond(now));
  estimator.Increment(100, now);
  EXPECT_EQ(26u, estimator.GetCountPerSecond(now));

  // Now wrap around to the start.
  now += base::Seconds(1);
  EXPECT_EQ(16u, estimator.GetCountPerSecond(now));
  estimator.Increment(100, now);
  EXPECT_EQ(26u, estimator.GetCountPerSecond(now));

  // Advance far into the future.
  now += base::Seconds(40);
  EXPECT_EQ(0u, estimator.GetCountPerSecond(now));
  estimator.Increment(100, now);
  EXPECT_EQ(100u, estimator.GetCountPerSecond(now));

  // Pretend that there is timeticks wrap around.
  now = base::TimeTicks();
  EXPECT_EQ(0u, estimator.GetCountPerSecond(now));
}

}  // namespace download
