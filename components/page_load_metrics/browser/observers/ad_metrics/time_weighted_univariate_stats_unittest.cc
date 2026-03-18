// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/time_weighted_univariate_stats.h"

#include <cmath>
#include <optional>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

class TimeWeightedUnivariateStatsTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TimeWeightedUnivariateStatsTest, NoData) {
  TimeWeightedUnivariateStats stats;
  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 0);
  EXPECT_DOUBLE_EQ(moments.variance, 0);
  EXPECT_DOUBLE_EQ(moments.skewness, 0);
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -3);
  EXPECT_FALSE(stats.maximum_value().has_value());
}

TEST_F(TimeWeightedUnivariateStatsTest, ImplicitZeroInitialValue) {
  TimeWeightedUnivariateStats stats;

  // Advance time without calling AddSample. This treats the starting value
  // as 0 for moments calculations.
  task_environment_.FastForwardBy(base::Microseconds(1));

  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 0);
  EXPECT_DOUBLE_EQ(moments.variance, 0);
  EXPECT_DOUBLE_EQ(moments.skewness, 0);
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -3);

  // Maximum value should still be explicitly std::nullopt if no value is set.
  EXPECT_FALSE(stats.maximum_value().has_value());
}

TEST_F(TimeWeightedUnivariateStatsTest, OneDataPoint) {
  TimeWeightedUnivariateStats stats;

  stats.AddSample(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 1);
  EXPECT_DOUBLE_EQ(moments.variance, 0);
  EXPECT_DOUBLE_EQ(moments.skewness, 0);
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -3);
}

TEST_F(TimeWeightedUnivariateStatsTest, TwoDataPoints_EqualWeight) {
  TimeWeightedUnivariateStats stats;

  stats.AddSample(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  stats.AddSample(2);
  task_environment_.FastForwardBy(base::Microseconds(1));

  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 1.5);
  EXPECT_DOUBLE_EQ(moments.variance, 0.25);
  EXPECT_DOUBLE_EQ(moments.skewness, 0);
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -2);
}

TEST_F(TimeWeightedUnivariateStatsTest, TwoDataPoints_UnequalWeight) {
  TimeWeightedUnivariateStats stats;

  stats.AddSample(1);
  task_environment_.FastForwardBy(base::Microseconds(3));

  stats.AddSample(2);
  task_environment_.FastForwardBy(base::Microseconds(1));

  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 5.0 / 4);
  EXPECT_DOUBLE_EQ(moments.variance, 3.0 / 16);
  EXPECT_DOUBLE_EQ(moments.skewness, 2.0 / std::sqrt(3));
  EXPECT_DOUBLE_EQ(moments.excess_kurtosis, -2.0 / 3);
}

TEST_F(TimeWeightedUnivariateStatsTest, MaximumValue) {
  TimeWeightedUnivariateStats stats;
  EXPECT_FALSE(stats.maximum_value().has_value());

  stats.AddSample(1);
  ASSERT_TRUE(stats.maximum_value().has_value());
  EXPECT_DOUBLE_EQ(stats.maximum_value().value(), 1);

  task_environment_.FastForwardBy(base::Microseconds(1));
  stats.AddSample(5);
  ASSERT_TRUE(stats.maximum_value().has_value());
  EXPECT_DOUBLE_EQ(stats.maximum_value().value(), 5);

  task_environment_.FastForwardBy(base::Microseconds(1));
  stats.AddSample(3);
  ASSERT_TRUE(stats.maximum_value().has_value());
  EXPECT_DOUBLE_EQ(stats.maximum_value().value(), 5);
}

TEST_F(TimeWeightedUnivariateStatsTest, PauseAndResume) {
  TimeWeightedUnivariateStats stats;

  stats.AddSample(1);
  task_environment_.FastForwardBy(base::Microseconds(2));

  stats.Pause();
  // This time shouldn't be accumulated since it's paused.
  task_environment_.FastForwardBy(base::Microseconds(5));

  stats.Resume();
  task_environment_.FastForwardBy(base::Microseconds(2));

  // Total active time is 4 microseconds at value 1.
  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();
  EXPECT_DOUBLE_EQ(moments.mean, 1);
}

TEST_F(TimeWeightedUnivariateStatsTest, AddSampleWhilePaused) {
  TimeWeightedUnivariateStats stats;

  stats.AddSample(1);
  task_environment_.FastForwardBy(base::Microseconds(2));

  stats.Pause();

  // Change the value while paused. The time spent paused should be ignored,
  // but the new value should be recognized once resumed.
  task_environment_.FastForwardBy(base::Microseconds(5));
  stats.AddSample(3);
  task_environment_.FastForwardBy(base::Microseconds(5));

  stats.Resume();
  task_environment_.FastForwardBy(base::Microseconds(2));

  // Total active time: 2us at value 1, and 2us at value 3.
  // Paused time (10us) is completely ignored.
  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();

  EXPECT_DOUBLE_EQ(moments.mean, 2.0);  // (1*2 + 3*2) / 4 = 8 / 4 = 2
}

TEST_F(TimeWeightedUnivariateStatsTest, RedundantPauseAndResume) {
  TimeWeightedUnivariateStats stats;

  stats.AddSample(2);
  task_environment_.FastForwardBy(base::Microseconds(2));

  // Redundant pauses should be a no-op.
  stats.Pause();
  stats.Pause();
  task_environment_.FastForwardBy(base::Microseconds(5));

  // Redundant resumes should be a no-op.
  stats.Resume();
  stats.Resume();
  task_environment_.FastForwardBy(base::Microseconds(2));

  // Total active time: 4us at value 2.
  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();
  EXPECT_DOUBLE_EQ(moments.mean, 2.0);
}

TEST_F(TimeWeightedUnivariateStatsTest, CalculateStatsFlushesOutstandingTime) {
  TimeWeightedUnivariateStats stats;

  stats.AddSample(5);
  task_environment_.FastForwardBy(base::Microseconds(4));

  // CalculateStats should automatically accumulate the 4us without us
  // needing to call a manual update, pause, or set new value.
  TimeWeightedUnivariateStats::DistributionMoments moments =
      stats.CalculateStats();
  EXPECT_DOUBLE_EQ(moments.mean, 5.0);
}

}  // namespace page_load_metrics
