// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/feature_usage/feature_usage_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_usage {

namespace {

const char kTestFeature[] = "TestFeature";
const char kTestMetric[] = "ChromeOS.FeatureUsage.TestFeature";

}  // namespace

class FeatureUsageMetricsTest : public ::testing::Test,
                                public FeatureUsageMetrics::Delegate {
 public:
  FeatureUsageMetricsTest() {
    FeatureUsageMetrics::RegisterPref(prefs_.registry(), kTestFeature);
    ResetHistogramTester();
    feature_usage_metrics_ = std::make_unique<FeatureUsageMetrics>(
        kTestFeature, &prefs_, this, env_.GetMockTickClock());
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

  TestingPrefServiceSimple prefs_;
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

TEST_F(FeatureUsageMetricsTest, DailyMetricsTest) {
  // Initial metrics should be reported on IntervalType::FIRST_RUN.
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 1);

  ResetHistogramTester();
  is_enabled_ = false;
  // Trigger IntervalType::DAY_ELAPSED event.
  env_.FastForwardBy(base::TimeDelta::FromHours(24));
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);

  ResetHistogramTester();
  is_eligible_ = false;
  // Trigger IntervalType::DAY_ELAPSED event.
  env_.FastForwardBy(base::TimeDelta::FromHours(24));
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);
}

}  // namespace feature_usage
