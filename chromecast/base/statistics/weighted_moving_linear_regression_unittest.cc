// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/statistics/weighted_moving_linear_regression.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(WeightedMovingLinearRegressionTest, NotEnoughSamples) {
  for (int num_samples = 0; num_samples <= 2; ++num_samples) {
    WeightedMovingLinearRegression linear(1e6);
    for (int s = 0; s < num_samples; ++s)
      linear.AddSample(s, s, 1.0);

    int64_t y = 12345;
    double error = 12345.0;
    EXPECT_FALSE(linear.EstimateY(0, &y, &error));
    EXPECT_EQ(12345, y);
    EXPECT_EQ(12345.0, error);

    double slope = 12345.0;
    EXPECT_FALSE(linear.EstimateSlope(&slope, &error));
    EXPECT_EQ(12345.0, slope);
    EXPECT_EQ(12345.0, error);
  }
}

TEST(WeightedMovingLinearRegressionTest, NoXVariance) {
  WeightedMovingLinearRegression linear(1e6);
  for (int s = 0; s < 10; ++s)
    linear.AddSample(0, s, 1.0);

  int64_t y = 12345;
  double error = 12345.0;
  EXPECT_FALSE(linear.EstimateY(0, &y, &error));
  EXPECT_EQ(12345, y);
  EXPECT_EQ(12345.0, error);

  double slope = 12345.0;
  EXPECT_FALSE(linear.EstimateSlope(&slope, &error));
  EXPECT_EQ(12345.0, slope);
  EXPECT_EQ(12345.0, error);
}

TEST(WeightedMovingLinearRegressionTest, ZeroWeight) {
  WeightedMovingLinearRegression linear(1e6);
  for (int s = 0; s < 10; ++s)
    linear.AddSample(s, s, 0.0);

  int64_t y = 12345;
  double error = 12345.0;
  EXPECT_FALSE(linear.EstimateY(0, &y, &error));
  EXPECT_EQ(12345, y);
  EXPECT_EQ(12345.0, error);

  double slope = 12345.0;
  EXPECT_FALSE(linear.EstimateSlope(&slope, &error));
  EXPECT_EQ(12345.0, slope);
  EXPECT_EQ(12345.0, error);
}

TEST(WeightedMovingLinearRegressionTest, SimpleLine) {
  WeightedMovingLinearRegression linear(1e6);
  for (int s = 0; s < 3; ++s)
    linear.AddSample(s, s, 1.0);

  int64_t y;
  double error;
  EXPECT_TRUE(linear.EstimateY(20, &y, &error));
  EXPECT_EQ(20, y);
  EXPECT_DOUBLE_EQ(0.0, error);

  double slope;
  EXPECT_TRUE(linear.EstimateSlope(&slope, &error));
  EXPECT_DOUBLE_EQ(1.0, slope);
  EXPECT_DOUBLE_EQ(0.0, error);
}

TEST(WeightedMovingLinearRegressionTest, SimpleLineHighX) {
  WeightedMovingLinearRegression linear(1e6);
  for (int s = 0; s < 10; ++s)
    linear.AddSample(1000000000 + s, s, 1.0);

  int64_t y;
  double error;
  EXPECT_TRUE(linear.EstimateY(0, &y, &error));
  EXPECT_EQ(-1000000000, y);
  EXPECT_DOUBLE_EQ(0.0, error);

  EXPECT_TRUE(linear.EstimateY(1000000020, &y, &error));
  EXPECT_EQ(20, y);
  EXPECT_DOUBLE_EQ(0.0, error);

  double slope;
  EXPECT_TRUE(linear.EstimateSlope(&slope, &error));
  EXPECT_DOUBLE_EQ(1.0, slope);
  EXPECT_DOUBLE_EQ(0.0, error);
}

TEST(WeightedMovingLinearRegressionTest, Weighted) {
  WeightedMovingLinearRegression linear(1e6);
  // Add some weight 1.0 points on the line y = x/2, and some weight 2.0 points
  // on the line y = x/2 + 4.5.
  for (int s = 0; s < 1000; ++s) {
    linear.AddSample(2 * s, s, 1.0);
    linear.AddSample(2 * s + 1, s + 5, 2.0);
  }

  // The resulting estimate should be y = x/2 + 3.
  int64_t y;
  double error;
  EXPECT_TRUE(linear.EstimateY(20, &y, &error));
  EXPECT_EQ(13, y);

  EXPECT_TRUE(linear.EstimateY(-20, &y, &error));
  EXPECT_EQ(-7, y);
  EXPECT_NEAR(0.0, error, 0.1);

  double slope;
  EXPECT_TRUE(linear.EstimateSlope(&slope, &error));
  EXPECT_NEAR(0.5, slope, 0.001);
  EXPECT_NEAR(0.0, error, 0.001);
}

TEST(WeightedMovingLinearRegressionTest, DropOldSamples) {
  WeightedMovingLinearRegression linear(1999);
  // First add some points that will fall outside of the window.
  for (int s = -1000; s < 0; ++s)
    linear.AddSample(s, 0, 1.0);

  // Add some weight 1.0 points on the line y = x/2, and some weight 2.0 points
  // on the line y = x/2 + 4.5.
  for (int s = 0; s < 1000; ++s) {
    linear.AddSample(2 * s, s, 1.0);
    linear.AddSample(2 * s + 1, s + 5, 2.0);
  }

  // The resulting estimate should be y = x/2 + 3.
  int64_t y;
  double error;
  EXPECT_TRUE(linear.EstimateY(20, &y, &error));
  EXPECT_EQ(13, y);

  EXPECT_TRUE(linear.EstimateY(-20, &y, &error));
  EXPECT_EQ(-7, y);
  EXPECT_NEAR(0.0, error, 0.1);

  double slope;
  EXPECT_TRUE(linear.EstimateSlope(&slope, &error));
  EXPECT_NEAR(0.5, slope, 0.001);
  EXPECT_NEAR(0.0, error, 0.001);
}

}  // namespace chromecast
