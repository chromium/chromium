// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "components/page_load_metrics/browser/observers/ad_metrics/univariate_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

TEST(UnivariateStatsTest, NoData) {
  UnivariateStats stats;
  UnivariateStats::DistributionMoments moments = stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 0);
  EXPECT_DOUBLE_EQ(moments.variance, 0);
  EXPECT_DOUBLE_EQ(moments.skewness, 0);
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -3);
}

TEST(UnivariateStatsTest, OneDataPoint) {
  UnivariateStats stats;
  stats.Accumulate(/*value=*/1, /*weight=*/1);

  UnivariateStats::DistributionMoments moments = stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 1);
  EXPECT_DOUBLE_EQ(moments.variance, 0);
  EXPECT_DOUBLE_EQ(moments.skewness, 0);
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -3);
}

TEST(UnivariateStatsTest, TwoDataPoints_EqualWeight) {
  UnivariateStats stats;
  stats.Accumulate(/*value=*/1, /*weight=*/1);
  stats.Accumulate(/*value=*/2, /*weight=*/1);

  UnivariateStats::DistributionMoments moments = stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 1.5);
  EXPECT_DOUBLE_EQ(moments.variance, 0.25);
  EXPECT_DOUBLE_EQ(moments.skewness, 0);
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -2);
}

TEST(UnivariateStatsTest, TwoDataPoints_UnequalWeight) {
  UnivariateStats stats;
  stats.Accumulate(/*value=*/1, /*weight=*/3);
  stats.Accumulate(/*value=*/2, /*weight=*/1);

  UnivariateStats::DistributionMoments moments = stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 5.0 / 4);
  EXPECT_DOUBLE_EQ(moments.variance, 3.0 / 16);
  EXPECT_DOUBLE_EQ(moments.skewness, 2.0 / std::sqrt(3));
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -2.0 / 3);
}

}  // namespace page_load_metrics
