// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "chromecast/base/statistics/weighted_moving_average.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(WeightedMovingAverageTest, NoSamples) {
  WeightedMovingAverage averager(0);

  int64_t avg = 12345;
  double error = 12345.0;
  EXPECT_FALSE(averager.Average(&avg, &error));
  EXPECT_EQ(12345, avg);
  EXPECT_EQ(12345.0, error);
}

TEST(WeightedMovingAverageTest, ZeroWeight) {
  WeightedMovingAverage averager(0);
  for (int s = 1; s <= 5; ++s)
    averager.AddSample(0, s, 0.0);

  int64_t avg = 12345;
  double error = 12345.0;
  EXPECT_FALSE(averager.Average(&avg, &error));
  EXPECT_EQ(12345, avg);
  EXPECT_EQ(12345.0, error);
}

TEST(WeightedMovingAverageTest, AverageOneValue) {
  int64_t value = 1;

  WeightedMovingAverage averager(0);
  averager.AddSample(0, value, 1.0);

  int64_t avg = 0;
  double error = 1.0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(value, avg);
  EXPECT_EQ(0.0, error);
}

TEST(WeightedMovingAverageTest, AverageSeveralUnweightedValues) {
  WeightedMovingAverage averager(0);
  for (int s = 1; s <= 5; ++s)
    averager.AddSample(0, s, 1.0);

  int64_t avg = 0;
  double error = 0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(3, avg);
  EXPECT_NEAR(sqrt(2) / sqrt(5), error, 1e-9);
}

TEST(WeightedMovingAverageTest, Clear) {
  WeightedMovingAverage averager(0);
  for (int s = 1; s <= 5; ++s)
    averager.AddSample(0, s, 1.0);

  int64_t avg = 0;
  double error = 0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(3, avg);
  EXPECT_NEAR(sqrt(2) / sqrt(5), error, 1e-9);

  averager.Clear();
  EXPECT_FALSE(averager.Average(&avg, &error));

  for (int s = 1; s <= 5; ++s)
    averager.AddSample(0, s, 1.0);

  avg = 0;
  error = 0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(3, avg);
  EXPECT_NEAR(sqrt(2) / sqrt(5), error, 1e-9);
}

TEST(WeightedMovingAverageTest, AverageSeveralWeightedValues) {
  WeightedMovingAverage averager(0);
  averager.AddSample(0, 1, 2.0);
  averager.AddSample(0, 2, 1.0);
  averager.AddSample(0, 3, 0.0);
  averager.AddSample(0, 4, 1.0);
  averager.AddSample(0, 5, 2.0);

  int64_t avg = 0;
  double error = 0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(3, avg);
  // <sum of weights>^2 / <sum of weights^2>
  double effective_sample_size = 36.0 / 10.0;
  EXPECT_NEAR(sqrt(3) / sqrt(effective_sample_size), error, 1e-9);
}

TEST(WeightedMovingAverageTest, DropOldValues) {
  WeightedMovingAverage averager(1);
  for (int s = 0; s < 10; ++s)
    averager.AddSample(s, 100, 5.0);

  averager.AddSample(10, 1, 1.0);
  averager.AddSample(11, 3, 1.0);

  int64_t avg = 0;
  double error = 0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(2, avg);
  EXPECT_DOUBLE_EQ(1.0 / sqrt(2), error);
}

TEST(WeightedMovingAverageTest, DropOldValuesUneven) {
  WeightedMovingAverage averager(5);
  for (int s = 0; s < 10; ++s)
    averager.AddSample(s * s, 100, 5.0);

  averager.AddSample(100, 1, 1.0);
  averager.AddSample(105, 3, 1.0);

  int64_t avg = 0;
  double error = 0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(2, avg);
  EXPECT_DOUBLE_EQ(1.0 / sqrt(2), error);
}

TEST(WeightedMovingAverageTest, DropOldValuesByAddingZeroWeightValues) {
  WeightedMovingAverage averager(5);
  for (int s = 0; s < 10; ++s)
    averager.AddSample(s, 1, 5.0);

  // Adding values with weight 0 still drops old values.
  for (int s = 11; s < 15; ++s)
    averager.AddSample(s, 100, 0.0);

  averager.AddSample(15, 10, 1.0);

  int64_t avg = 0;
  double error = 0;
  EXPECT_TRUE(averager.Average(&avg, &error));
  EXPECT_EQ(10, avg);
  EXPECT_DOUBLE_EQ(0.0, error);
}

}  // namespace chromecast
