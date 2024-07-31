// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reporting/metrics/periodic_collector.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr char kEnableSettingPath[] = "enable_path";
constexpr char kRateSettingPath[] = "rate_path";

constexpr base::TimeDelta interval = base::Milliseconds(10000);

class PeriodicCollectorTest : public ::testing::Test {
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

TEST_F(PeriodicCollectorTest, InitiallyEnabled) {
  settings_->SetReportingEnabled(kEnableSettingPath, true);
  settings_->SetInteger(kRateSettingPath, interval.InMilliseconds());

  MetricData metric_data_list[5];
  metric_data_list[0].mutable_telemetry_data();
  metric_data_list[1].mutable_info_data();
  metric_data_list[2].mutable_event_data();
  metric_data_list[3].mutable_telemetry_data();
  metric_data_list[3].mutable_event_data();
  metric_data_list[4].mutable_info_data();
  metric_data_list[4].mutable_event_data();

  sampler_->SetMetricData(metric_data_list[0]);
  PeriodicCollector collector(sampler_.get(), metric_report_queue_.get(),
                              settings_.get(), kEnableSettingPath,
                              /*setting_enabled_default_value=*/false,
                              kRateSettingPath, interval / 2);

  // One initial collection at startup.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  // Expected calls count initialized to 1 to reflect the initial collection.
  int expected_collect_calls = 1;
  for (int i = 0; i < 2; ++i) {
    sampler_->SetMetricData(metric_data_list[i + 1]);
    // 5 secs elapsed, no new data collected.
    task_environment_.FastForwardBy(interval / 2);
    EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collect_calls));

    ++expected_collect_calls;
    // 10 secs elapsed, data should be collected.
    task_environment_.FastForwardBy(interval / 2);
    EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collect_calls));
    ;
  }

  sampler_->SetMetricData(metric_data_list[3]);
  settings_->SetReportingEnabled(kEnableSettingPath, false);
  // Setting disabled, no data should be collected.
  task_environment_.FastForwardBy(interval);
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collect_calls));

  settings_->SetReportingEnabled(kEnableSettingPath, true);
  // Initial collection at policy enablement.
  ++expected_collect_calls;
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collect_calls));

  sampler_->SetMetricData(metric_data_list[4]);
  // Setting enabled, data should be collected after interval.
  task_environment_.FastForwardBy(interval / 2);
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collect_calls));
  ++expected_collect_calls;
  task_environment_.FastForwardBy(interval / 2);
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collect_calls));

  for (const auto& metric_data : metric_data_list) {
    MetricData metric_data_reported =
        metric_report_queue_->GetMetricDataReported();
    EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
    EXPECT_THAT(metric_data_reported.has_telemetry_data(),
                Eq(metric_data.has_telemetry_data()));
    EXPECT_THAT(metric_data_reported.has_info_data(),
                Eq(metric_data.has_info_data()));
    EXPECT_THAT(metric_data_reported.has_event_data(),
                Eq(metric_data.has_event_data()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(
      PeriodicCollector::kNoMetricDataMetricsName, /*expected_count=*/0);
}

TEST_F(PeriodicCollectorTest, InitiallyEnabled_Delayed) {
  constexpr base::TimeDelta init_delay = base::Minutes(1);

  settings_->SetReportingEnabled(kEnableSettingPath, true);
  settings_->SetInteger(kRateSettingPath, interval.InMilliseconds());

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  sampler_->SetMetricData(metric_data);
  PeriodicCollector collector(sampler_.get(), metric_report_queue_.get(),
                              settings_.get(), kEnableSettingPath,
                              /*setting_enabled_default_value=*/false,
                              kRateSettingPath, interval / 2,
                              /*rate_unit_to_ms=*/1, init_delay);

  // No collection since the init delay is not elapsed.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  task_environment_.FastForwardBy(interval);

  // Only interval is elapsed which is less than init delay so still no
  // collection.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  collector.Collect(/*is_event_driven=*/true);

  // `init_delay` not elapsed but manual collection is triggered.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));
  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_TRUE(metric_data_reported.telemetry_data().is_event_driven());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());

  task_environment_.FastForwardBy(init_delay - interval);

  // One initial collection when the init delay is elapsed.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(2));

  metric_data_reported = metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_FALSE(metric_data_reported.telemetry_data().is_event_driven());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(metric_report_queue_->IsEmpty());

  sampler_->SetMetricData(std::nullopt);
  histogram_tester_.ExpectTotalCount(
      PeriodicCollector::kNoMetricDataMetricsName, /*expected_count=*/0);
}

TEST_F(PeriodicCollectorTest, NoMetricData) {
  settings_->SetReportingEnabled(kEnableSettingPath, true);
  settings_->SetInteger(kRateSettingPath, interval.InMilliseconds());

  sampler_->SetMetricData(std::nullopt);
  PeriodicCollector collector(sampler_.get(), metric_report_queue_.get(),
                              settings_.get(), kEnableSettingPath,
                              /*setting_enabled_default_value=*/false,
                              kRateSettingPath, interval / 2);

  // One initial collection at startup.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectBucketCount(
      PeriodicCollector::kNoMetricDataMetricsName,
      metric_report_queue_->GetDestination(), /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      PeriodicCollector::kNoMetricDataMetricsName, /*expected_count=*/1);
}

TEST_F(PeriodicCollectorTest, InitiallyDisabled) {
  settings_->SetReportingEnabled(kEnableSettingPath, false);
  settings_->SetInteger(kRateSettingPath, interval.InMilliseconds());

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  sampler_->SetMetricData(std::move(metric_data));

  PeriodicCollector collector(sampler_.get(), metric_report_queue_.get(),
                              settings_.get(), kEnableSettingPath,
                              /*setting_enabled_default_value=*/false,
                              kRateSettingPath, interval / 2);

  task_environment_.FastForwardBy(interval);
  // Setting is disabled, no data collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  collector.Collect(/*is_event_driven=*/true);

  // Manual collection is triggered but reporting is disabled.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 0);

  settings_->SetReportingEnabled(kEnableSettingPath, true);
  // One initial collection at policy enablement.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_FALSE(metric_data_reported.telemetry_data().has_is_event_driven());

  task_environment_.FastForwardBy(interval / 2);

  collector.Collect(/*is_event_driven=*/true);

  // `interval` not elapsed but manual collection is triggered.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(2));
  metric_data_reported = metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_TRUE(metric_data_reported.telemetry_data().is_event_driven());

  task_environment_.FastForwardBy(interval / 2);
  // 1 collection at policy enablement + 1 manual collection  + 1 collection
  // after interval.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(3));

  metric_data_reported = metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  EXPECT_FALSE(metric_data_reported.telemetry_data().has_is_event_driven());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(
      PeriodicCollector::kNoMetricDataMetricsName, /*expected_count=*/0);
}

TEST_F(PeriodicCollectorTest, DefaultEnabled) {
  settings_->SetInteger(kRateSettingPath, interval.InMilliseconds());

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  sampler_->SetMetricData(std::move(metric_data));
  PeriodicCollector collector(sampler_.get(), metric_report_queue_.get(),
                              settings_.get(), "invalid/path",
                              /*setting_enabled_default_value=*/true,
                              kRateSettingPath, interval / 2);

  // One initial collection at startup.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  metric_data.Clear();
  metric_data.mutable_event_data();
  sampler_->SetMetricData(std::move(metric_data));
  // 10 secs elapsed, data should be collected.
  task_environment_.FastForwardBy(interval);
  // 1 collection at startup + 1 collection after interval.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(2));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  metric_data_reported = metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_FALSE(metric_data_reported.has_telemetry_data());
  EXPECT_TRUE(metric_data_reported.has_event_data());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(
      PeriodicCollector::kNoMetricDataMetricsName, /*expected_count=*/0);
}

TEST_F(PeriodicCollectorTest, DefaultDisabled) {
  settings_->SetInteger(kRateSettingPath, interval.InMilliseconds());

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  PeriodicCollector collector(sampler_.get(), metric_report_queue_.get(),
                              settings_.get(), "invalid/path",
                              /*setting_enabled_default_value=*/false,
                              kRateSettingPath, interval / 2);

  sampler_->SetMetricData(std::move(metric_data));
  task_environment_.FastForwardBy(interval);
  base::RunLoop().RunUntilIdle();

  // Setting is disabled by default, no data collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(
      PeriodicCollector::kNoMetricDataMetricsName, /*expected_count=*/0);
}
}  // namespace
}  // namespace reporting
