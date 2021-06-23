// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/feature_usage/feature_usage_metrics.h"

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_usage {

namespace {

const char kTestFeature[] = "TestFeature";
const char kTestMetric[] = "ChromeOS.FeatureUsage.TestFeature";
const char kTestUsetimeMetric[] = "ChromeOS.FeatureUsage.TestFeature.Usetime";

}  // namespace

class FeatureUsageMetricsTest : public ::testing::Test,
                                public FeatureUsageMetrics::Delegate {
 public:
  FeatureUsageMetricsTest() {
    ResetHistogramTester();
    feature_usage_metrics_ = std::make_unique<FeatureUsageMetrics>(
        kTestFeature, this, env_.GetMockTickClock());
  }

  // FeatureUsageMetrics::Delegate:
  bool IsEligible() const override { return is_eligible_; }
  bool IsEnabled() const override { return is_enabled_; }

 protected:
  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  base::test::TaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  bool is_eligible_ = true;
  bool is_enabled_ = true;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<FeatureUsageMetrics> feature_usage_metrics_;
};

TEST_F(FeatureUsageMetricsTest, RecordUsageWithSuccess) {
  feature_usage_metrics_->RecordUsage(/*success=*/true);
  histogram_tester_->ExpectBucketCount(
      kTestMetric,
      static_cast<int>(FeatureUsageMetrics::Event::kUsedWithSuccess), 1);
}

TEST_F(FeatureUsageMetricsTest, RecordUsageWithFailure) {
  feature_usage_metrics_->RecordUsage(/*success=*/false);
  histogram_tester_->ExpectBucketCount(
      kTestMetric,
      static_cast<int>(FeatureUsageMetrics::Event::kUsedWithFailure), 1);
}

TEST_F(FeatureUsageMetricsTest, RecordUsetime) {
  const base::TimeDelta use_time = base::TimeDelta::FromSeconds(10);
  feature_usage_metrics_->RecordUsage(/*success=*/true);
  feature_usage_metrics_->StartSuccessfulUsage();
  env_.FastForwardBy(use_time);
  feature_usage_metrics_->StopSuccessfulUsage();
  histogram_tester_->ExpectUniqueTimeSample(kTestUsetimeMetric, use_time, 1);
}

TEST_F(FeatureUsageMetricsTest, RecordLongUsetime) {
  size_t repeated_periods = 4;
  const base::TimeDelta extra_small_use_time = base::TimeDelta::FromMinutes(3);
  const base::TimeDelta use_time =
      FeatureUsageMetrics::kRepeatedInterval * repeated_periods +
      extra_small_use_time;

  feature_usage_metrics_->RecordUsage(/*success=*/true);
  feature_usage_metrics_->StartSuccessfulUsage();
  env_.FastForwardBy(use_time);
  feature_usage_metrics_->StopSuccessfulUsage();
  histogram_tester_->ExpectTimeBucketCount(
      kTestUsetimeMetric, FeatureUsageMetrics::kRepeatedInterval,
      repeated_periods);
  histogram_tester_->ExpectTimeBucketCount(kTestUsetimeMetric,
                                           extra_small_use_time, 1);
}

TEST_F(FeatureUsageMetricsTest, PeriodicMetricsTest) {
  // Periodic metrics are not reported on creation.
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);

  ResetHistogramTester();
  // Trigger initial periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kInitialInterval);

  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 1);

  ResetHistogramTester();
  is_enabled_ = false;
  // Trigger repeated periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kRepeatedInterval);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);

  ResetHistogramTester();
  is_eligible_ = false;
  // Trigger repeated periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kRepeatedInterval);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);
}

}  // namespace feature_usage
