// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/exponential_moving_average.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(ExponentialMovingAverageTest, AppendDatum) {
  ExponentialMovingAverage avg(0.5);
  EXPECT_EQ(0.5, avg.alpha());
  EXPECT_EQ(0u, avg.num_datums());
  EXPECT_EQ(0.0, avg.value());

  avg.AppendDatum(10.0);
  EXPECT_EQ(1u, avg.num_datums());
  EXPECT_EQ(10.0, avg.value());

  avg.AppendDatum(5.0);
  EXPECT_EQ(2u, avg.num_datums());
  EXPECT_EQ(7.5, avg.value());

  avg.AppendDatum(2.0);
  EXPECT_EQ(3u, avg.num_datums());
  EXPECT_EQ(4.75, avg.value());
}

TEST(ExponentialMovingAverageTest, PrependDatum) {
  ExponentialMovingAverage avg(0.5);
  EXPECT_EQ(0u, avg.num_datums());

  avg.PrependDatum(2.0);
  EXPECT_EQ(1u, avg.num_datums());
  EXPECT_EQ(2.0, avg.value());

  avg.PrependDatum(5.0);
  EXPECT_EQ(2u, avg.num_datums());
  EXPECT_EQ(3.5, avg.value());

  avg.PrependDatum(10.0);
  EXPECT_EQ(3u, avg.num_datums());
  EXPECT_EQ(4.75, avg.value());
}

TEST(ExponentialMovingAverageTest, ContinueFromPreviousSession) {
  ExponentialMovingAverage avg1(0.75);
  avg1.AppendDatum(10.0);
  avg1.AppendDatum(5.0);
  avg1.AppendDatum(3.5);

  // Get the value of the moving average to this point.
  float start_avg = avg1.value();

  // Create a new moving average and add some samples to both.
  ExponentialMovingAverage avg2(0.75);

  avg1.AppendDatum(9.25);
  avg2.AppendDatum(9.25);

  avg1.AppendDatum(3.33);
  avg2.AppendDatum(3.33);
  EXPECT_NE(avg1.value(), avg2.value());

  // Now prepend the start value of the first histogram to the second one,
  // which should bring them into line.
  avg2.PrependDatum(start_avg);
  EXPECT_EQ(avg1.value(), avg2.value());
}

TEST(ExponentialMovingAverageTest, Clear) {
  ExponentialMovingAverage avg(0.25);
  EXPECT_EQ(0.25, avg.alpha());

  avg.AppendDatum(5.25);
  avg.AppendDatum(3.25);
  avg.PrependDatum(2.25);

  float old_value = avg.value();

  EXPECT_NE(0.0, old_value);
  EXPECT_NE(0u, avg.num_datums());

  // Clear it, and make sure it's reset proper.
  avg.Clear();
  EXPECT_EQ(0.0, avg.value());
  EXPECT_EQ(0u, avg.num_datums());

  avg.AppendDatum(5.25);
  avg.AppendDatum(3.25);
  avg.PrependDatum(2.25);

  EXPECT_EQ(old_value, avg.value());
}

}  // namespace performance_manager
