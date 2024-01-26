// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/one_shot_collector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr char kEnableSettingPath[] = "enable_path";

class OneShotCollectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    settings_ = std::make_unique<test::FakeReportingSettings>();
    sampler_ = std::make_unique<test::FakeSampler>();
    metric_report_queue_ = std::make_unique<test::FakeMetricReportQueue>();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<test::FakeReportingSettings> settings_;
  std::unique_ptr<test::FakeSampler> sampler_;
  std::unique_ptr<test::FakeMetricReportQueue> metric_report_queue_;
  base::HistogramTester histogram_tester_;
};

TEST_F(OneShotCollectorTest, InitiallyEnabled) {
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_->SetMetricData(std::move(metric_data));
  base::test::TestFuture<Status> test_future;

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false,
                             test_future.GetCallback());

  // Setting is initially enabled, data is being collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  settings_->SetReportingEnabled(kEnableSettingPath, false);
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  // No more data should be collected even if the setting was disabled then
  // re-enabled.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));
  EXPECT_TRUE(test_future.Wait());

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_FALSE(metric_data_reported.telemetry_data().has_is_event_driven());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());

  settings_->SetReportingEnabled(kEnableSettingPath, false);
  collector.Collect(/*is_event_driven=*/true);

  // No new data collection, setting is disabled.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  settings_->SetReportingEnabled(kEnableSettingPath, true);
  collector.Collect(/*is_event_driven=*/true);

  // Number of collection calls increased by one, setting is enabled and manual
  // collection called.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(2));

  metric_data_reported = metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_TRUE(metric_data_reported.telemetry_data().is_event_driven());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(OneShotCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(OneShotCollectorTest, InitiallyEnabled_Delayed) {
  constexpr base::TimeDelta init_delay = base::Minutes(2);
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_->SetMetricData(std::move(metric_data));
  base::test::TestFuture<Status> test_future;

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false,
                             init_delay, test_future.GetCallback());

  base::TimeDelta elapsed = base::Seconds(90);
  task_environment_.FastForwardBy(elapsed);

  // `init_delay` is not elapsed yet.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  collector.Collect(/*is_event_driven=*/true);

  // Data manually collected before `init_delay`.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_TRUE(metric_data_reported.telemetry_data().is_event_driven());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());

  task_environment_.FastForwardBy(init_delay - elapsed);

  // Setting is initially enabled and `init_delay` elapsed, data is collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(2));

  settings_->SetReportingEnabled(kEnableSettingPath, false);
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  // No more data should be collected even if the setting was disabled then
  // re-enabled.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(2));
  EXPECT_TRUE(test_future.Wait());

  metric_data_reported = metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(OneShotCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(OneShotCollectorTest, NoMetricData) {
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  sampler_->SetMetricData(std::nullopt);

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false);

  // Setting is initially enabled, data is being collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectBucketCount(
      OneShotCollector::kNoMetricDataMetricsName,
      metric_report_queue_->GetDestination(), /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(OneShotCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/1);
}

TEST_F(OneShotCollectorTest, InitiallyDisabled) {
  settings_->SetReportingEnabled(kEnableSettingPath, false);

  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false);

  // Setting is initially disabled, no data is collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  settings_->SetReportingEnabled(kEnableSettingPath, true);

  // Setting is enabled, data is being collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  settings_->SetReportingEnabled(kEnableSettingPath, false);
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  // No more data should be collected even if the setting was disabled then
  // re-enabled.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_info_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(OneShotCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(OneShotCollectorTest, InitiallyDisabled_Delayed) {
  constexpr base::TimeDelta init_delay = base::Minutes(1);
  settings_->SetReportingEnabled(kEnableSettingPath, false);

  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false,
                             init_delay);

  task_environment_.FastForwardBy(init_delay);

  // Setting is initially disabled, no data is collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  settings_->SetReportingEnabled(kEnableSettingPath, true);

  // Setting is enabled, data is being collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  settings_->SetReportingEnabled(kEnableSettingPath, false);
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  // No more data should be collected even if the setting was disabled then
  // re-enabled.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_info_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(OneShotCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(OneShotCollectorTest, DefaultEnabled) {
  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));
  base::test::TestFuture<Status> test_future;

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), "invalid/path",
                             /*setting_enabled_default_value=*/true,
                             test_future.GetCallback());

  // Setting is enabled by default, data is being collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));
  EXPECT_TRUE(test_future.Wait());

  const auto& metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_info_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(OneShotCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(OneShotCollectorTest, DefaultDisabled) {
  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false);
  base::RunLoop().RunUntilIdle();

  // Setting is disabled by default, no data is collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(OneShotCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}
}  // namespace
}  // namespace reporting
