// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

#include <optional>

#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::feature_usage {

namespace {

const char kTestFeature[] = "TestFeature";
const char kTestMetric[] = "ChromeOS.FeatureUsage.TestFeature";
const char kTestUsetimeMetric[] = "ChromeOS.FeatureUsage.TestFeature.Usetime";
constexpr base::TimeDelta kDefaultUseTime = base::Minutes(10);

}  // namespace

class FeatureUsageMetricsTest : public ::testing::Test,
                                public FeatureUsageMetrics::Delegate {
 public:
  FeatureUsageMetricsTest() {
    if (auto* power_monitor = base::PowerMonitor::GetInstance();
        !power_monitor->IsInitialized()) {
      power_monitor->Initialize(
          std::make_unique<base::PowerMonitorDeviceSource>());
    }

    ResetHistogramTester();

    feature_usage_metrics_ = std::make_unique<FeatureUsageMetrics>(
        kTestFeature, this, env_.GetMockClock(), env_.GetMockTickClock());
  }

  // FeatureUsageMetrics::Delegate:
  bool IsEligible() const override { return is_eligible_; }
  std::optional<bool> IsAccessible() const override { return is_accessible_; }
  bool IsEnabled() const override { return is_enabled_; }

 protected:
  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  base::test::TaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  bool is_eligible_ = true;
  std::optional<bool> is_accessible_;
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
  feature_usage_metrics_->RecordUsage(/*success=*/true);
  feature_usage_metrics_->StartSuccessfulUsage();
  env_.FastForwardBy(kDefaultUseTime);
  feature_usage_metrics_->StopSuccessfulUsage();
  histogram_tester_->ExpectUniqueTimeSample(kTestUsetimeMetric, kDefaultUseTime,
                                            1);
}

TEST_F(FeatureUsageMetricsTest, RecordLongUsetime) {
  size_t repeated_periods = 4;
  const base::TimeDelta extra_small_use_time = base::Minutes(3);
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
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);

  ResetHistogramTester();
  // Trigger initial periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kInitialInterval);

  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 1);

  ResetHistogramTester();
  is_enabled_ = false;
  // Trigger repeated periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kRepeatedInterval);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);

  ResetHistogramTester();
  is_eligible_ = false;
  // Trigger repeated periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kRepeatedInterval);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);
}

TEST_F(FeatureUsageMetricsTest, PeriodicWithAccessibleMetricsTest) {
  is_accessible_ = true;
  ResetHistogramTester();
  // Trigger initial periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kInitialInterval);

  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 1);

  ResetHistogramTester();
  is_enabled_ = false;
  // Trigger repeated periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kRepeatedInterval);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);

  ResetHistogramTester();
  is_accessible_ = false;
  // Trigger repeated periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kRepeatedInterval);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);

  ResetHistogramTester();
  is_eligible_ = false;
  // Trigger repeated periodic metrics report.
  env_.FastForwardBy(FeatureUsageMetrics::kRepeatedInterval);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kAccessible),
      0);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 0);
}

TEST_F(FeatureUsageMetricsTest, ReportUseTimeOnShutdown) {
  feature_usage_metrics_->RecordUsage(/*success=*/true);
  feature_usage_metrics_->StartSuccessfulUsage();
  env_.FastForwardBy(kDefaultUseTime);
  feature_usage_metrics_.reset();
  histogram_tester_->ExpectUniqueTimeSample(kTestUsetimeMetric, kDefaultUseTime,
                                            1);
}

TEST_F(FeatureUsageMetricsTest, ReportPeriodicOnSuspend) {
  base::PowerMonitorDeviceSource::HandleSystemSuspending();
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 1);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 1);

  // Undo global changes.
  base::PowerMonitorDeviceSource::HandleSystemResumed();
}

TEST_F(FeatureUsageMetricsTest, ReportUseTimeOnSuspend) {
  feature_usage_metrics_->RecordUsage(/*success=*/true);
  feature_usage_metrics_->StartSuccessfulUsage();
  env_.FastForwardBy(kDefaultUseTime);

  base::PowerMonitorDeviceSource::HandleSystemSuspending();
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectUniqueTimeSample(kTestUsetimeMetric, kDefaultUseTime,
                                            1);

  // Undo global changes.
  base::PowerMonitorDeviceSource::HandleSystemResumed();
}

TEST_F(FeatureUsageMetricsTest, SuspensionTimeNotReported) {
  feature_usage_metrics_->RecordUsage(/*success=*/true);
  feature_usage_metrics_->StartSuccessfulUsage();
  env_.FastForwardBy(kDefaultUseTime);
  base::PowerMonitorDeviceSource::HandleSystemSuspending();
  base::RunLoop().RunUntilIdle();

  // Time during suspension must not be reported.
  env_.AdvanceClock(FeatureUsageMetrics::kRepeatedInterval * 1.33);

  base::PowerMonitorDeviceSource::HandleSystemResumed();
  base::RunLoop().RunUntilIdle();

  env_.FastForwardBy(kDefaultUseTime);
  feature_usage_metrics_->StopSuccessfulUsage();

  // Time reported before suspend.
  histogram_tester_->ExpectTimeBucketCount(kTestUsetimeMetric, kDefaultUseTime,
                                           1);
  // Time reported after suspend on initial interval.
  histogram_tester_->ExpectTimeBucketCount(
      kTestUsetimeMetric, FeatureUsageMetrics::kInitialInterval, 1);
  // Time reported on StopSuccessfulUsage.
  histogram_tester_->ExpectTimeBucketCount(
      kTestUsetimeMetric,
      kDefaultUseTime - FeatureUsageMetrics::kInitialInterval, 1);

  histogram_tester_->ExpectTotalCount(kTestUsetimeMetric, 3);

  // isEligible and isEnabled should contain 3 records:
  //  1. Reported on startup after kInitialInterval.
  //  2. Reported OnSuspend
  //  3. Reported OnResume after kInitialInterval.
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEligible), 3);
  histogram_tester_->ExpectBucketCount(
      kTestMetric, static_cast<int>(FeatureUsageMetrics::Event::kEnabled), 3);
}

}  // namespace ash::feature_usage
