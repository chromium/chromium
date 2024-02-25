// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/manual_collector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr char kEnableSettingPath[] = "enable_path";

class ManualCollectorTest : public testing::Test {
 protected:
  MetricData GetMetricData() {
    MetricData metric_data;
    metric_data.mutable_telemetry_data();
    return metric_data;
  }

  void VerifyMetricData(MetricData metric_data, bool is_event_driven) const {
    EXPECT_TRUE(metric_data.has_timestamp_ms());
    EXPECT_TRUE(metric_data.has_telemetry_data());
    EXPECT_THAT(is_event_driven,
                Eq(metric_data.telemetry_data().is_event_driven()));
    EXPECT_TRUE(metric_report_queue_->IsEmpty());
  }

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

TEST_F(ManualCollectorTest, InitiallyEnabled) {
  // Initially enable settings
  settings_->SetReportingEnabled(kEnableSettingPath, true);
  MetricData metric_data = GetMetricData();
  sampler_->SetMetricData(std::move(metric_data));

  ManualCollector collector(sampler_.get(), metric_report_queue_.get(),
                            settings_.get(), kEnableSettingPath,
                            /*setting_enabled_default_value=*/false);

  // We haven't manually initiated collection
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  // Manually collect event driven data
  static constexpr bool is_event_driven = true;
  collector.Collect(is_event_driven);

  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  VerifyMetricData(metric_data_reported, is_event_driven);
  histogram_tester_.ExpectTotalCount(ManualCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(ManualCollectorTest, InitiallyDisabled) {
  // Initially disable settings
  settings_->SetReportingEnabled(kEnableSettingPath, false);

  MetricData metric_data = GetMetricData();

  sampler_->SetMetricData(std::move(metric_data));

  ManualCollector collector(sampler_.get(), metric_report_queue_.get(),
                            settings_.get(), kEnableSettingPath,
                            /*setting_enabled_default_value=*/false);

  // We haven't manually initiated collection
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  static constexpr bool is_event_driven = true;
  collector.Collect(is_event_driven);

  // No data should be collected since settings are disabled
  EXPECT_TRUE(metric_report_queue_->IsEmpty());

  // Enable settings
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  collector.Collect(is_event_driven);

  // Setting is enabled, data is being collected.
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  VerifyMetricData(metric_data_reported, is_event_driven);
  histogram_tester_.ExpectTotalCount(ManualCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(ManualCollectorTest, NoMetricData) {
  settings_->SetReportingEnabled(kEnableSettingPath, true);

  sampler_->SetMetricData(std::nullopt);

  ManualCollector collector(sampler_.get(), metric_report_queue_.get(),
                            settings_.get(), kEnableSettingPath,
                            /*setting_enabled_default_value=*/false);

  // We haven't manually initiated collection
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  collector.Collect(/*is_event_driven=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));
  ASSERT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectBucketCount(ManualCollector::kNoMetricDataMetricsName,
                                      metric_report_queue_->GetDestination(),
                                      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(ManualCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/1);
}

TEST_F(ManualCollectorTest, DefaultEnabled) {
  MetricData metric_data = GetMetricData();
  sampler_->SetMetricData(std::move(metric_data));

  // Setting enabled by default via constructor
  ManualCollector collector(sampler_.get(), metric_report_queue_.get(),
                            settings_.get(), "invalid/path",
                            /*setting_enabled_default_value=*/true);

  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));

  static constexpr bool is_event_driven = true;
  collector.Collect(is_event_driven);

  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(1));

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  VerifyMetricData(metric_data_reported, is_event_driven);
  histogram_tester_.ExpectTotalCount(ManualCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}

TEST_F(ManualCollectorTest, DefaultDisabled) {
  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));

  ManualCollector collector(sampler_.get(), metric_report_queue_.get(),
                            settings_.get(), "invalid/path",
                            /*setting_enabled_default_value=*/false);
  collector.Collect(/*is_event_driven=*/false);

  // Shouldn't be able to call the sampler since setting is disabled
  EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(0));
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
  histogram_tester_.ExpectTotalCount(ManualCollector::kNoMetricDataMetricsName,
                                     /*expected_count=*/0);
}
}  // namespace
}  // namespace reporting
