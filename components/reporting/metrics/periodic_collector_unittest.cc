// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/periodic_collector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace {

constexpr char kEnableSettingPath[] = "enable_path";
constexpr char kRateSettingPath[] = "rate_path";

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
};

TEST_F(PeriodicCollectorTest, InitiallyEnabled) {
  constexpr int interval = 10000;
  settings_->SetBoolean(kEnableSettingPath, true);
  settings_->SetInteger(kRateSettingPath, interval);

  MetricData metric_data_list[5];
  metric_data_list[0].mutable_telemetry_data();
  metric_data_list[1].mutable_info_data();
  metric_data_list[2].mutable_event_data();
  metric_data_list[3].mutable_telemetry_data();
  metric_data_list[3].mutable_event_data();
  metric_data_list[4].mutable_info_data();
  metric_data_list[4].mutable_event_data();

  sampler_->SetMetricData(metric_data_list[0]);
  PeriodicCollector collector(
      sampler_.get(), metric_report_queue_.get(), settings_.get(),
      kEnableSettingPath, /*setting_enabled_default_value=*/false,
      kRateSettingPath, base::Milliseconds(interval / 2));

  // One initial collection at startup.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  // Expected calls count initialized to 1 to reflect the initial collection.
  int expected_collect_calls = 1;
  for (int i = 0; i < 2; ++i) {
    sampler_->SetMetricData(metric_data_list[i + 1]);
    // 5 secs elapsed, no new data collected.
    task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
    EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);

    ++expected_collect_calls;
    // 10 secs elapsed, data should be collected.
    task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
    EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);
    ;
  }

  sampler_->SetMetricData(metric_data_list[3]);
  settings_->SetBoolean(kEnableSettingPath, false);
  // Setting disabled, no data should be collected.
  task_environment_.FastForwardBy(base::Milliseconds(interval));
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);

  settings_->SetBoolean(kEnableSettingPath, true);
  // Initial collection at policy enablement.
  ++expected_collect_calls;
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);

  sampler_->SetMetricData(metric_data_list[4]);
  // Setting enabled, data should be collected after interval.
  task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);
  ++expected_collect_calls;
  task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);

  for (const auto& metric_data : metric_data_list) {
    MetricData metric_data_reported =
        metric_report_queue_->GetMetricDataReported();
    EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
    EXPECT_EQ(metric_data_reported.has_telemetry_data(),
              metric_data.has_telemetry_data());
    EXPECT_EQ(metric_data_reported.has_info_data(),
              metric_data.has_info_data());
    EXPECT_EQ(metric_data_reported.has_event_data(),
              metric_data.has_event_data());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(PeriodicCollectorTest, NoMetricData) {
  constexpr int interval = 10000;
  settings_->SetBoolean(kEnableSettingPath, true);
  settings_->SetInteger(kRateSettingPath, interval);

  sampler_->SetMetricData(absl::nullopt);

  PeriodicCollector collector(
      sampler_.get(), metric_report_queue_.get(), settings_.get(),
      kEnableSettingPath, /*setting_enabled_default_value=*/false,
      kRateSettingPath, base::Milliseconds(interval / 2));

  // One initial collection at startup.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(PeriodicCollectorTest, InitiallyDisabled) {
  constexpr int interval = 10000;
  settings_->SetBoolean(kEnableSettingPath, false);
  settings_->SetInteger(kRateSettingPath, interval);

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  sampler_->SetMetricData(std::move(metric_data));

  PeriodicCollector collector(
      sampler_.get(), metric_report_queue_.get(), settings_.get(),
      kEnableSettingPath, /*setting_enabled_default_value=*/false,
      kRateSettingPath, base::Milliseconds(interval / 2));

  task_environment_.FastForwardBy(base::Milliseconds(interval));
  // Setting is disabled, no data collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 0);

  settings_->SetBoolean(kEnableSettingPath, true);
  // One initial collection at policy enablement.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  task_environment_.FastForwardBy(base::Milliseconds(interval));
  // 1 collection at policy enablement + 1 collection after interval.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 2);

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
  metric_data_reported = metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(PeriodicCollectorTest, DefaultEnabled) {
  constexpr int interval = 10000;
  settings_->SetInteger(kRateSettingPath, interval);

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  sampler_->SetMetricData(std::move(metric_data));
  PeriodicCollector collector(
      sampler_.get(), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/true, kRateSettingPath,
      base::Milliseconds(interval / 2));

  // One initial collection at startup.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  metric_data.Clear();
  metric_data.mutable_event_data();
  sampler_->SetMetricData(std::move(metric_data));
  // 10 secs elapsed, data should be collected.
  task_environment_.FastForwardBy(base::Milliseconds(interval));
  // 1 collection at startup + 1 collection after interval.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 2);

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
}

TEST_F(PeriodicCollectorTest, DefaultDisabled) {
  constexpr int interval = 10000;
  settings_->SetInteger(kRateSettingPath, interval);

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  PeriodicCollector collector(
      sampler_.get(), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/false, kRateSettingPath,
      base::Milliseconds(interval / 2));

  sampler_->SetMetricData(std::move(metric_data));
  task_environment_.FastForwardBy(base::Milliseconds(interval));
  base::RunLoop().RunUntilIdle();

  // Setting is disabled by default, no data collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 0);
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}
}  // namespace
}  // namespace reporting
