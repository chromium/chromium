// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/one_shot_collector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
};

TEST_F(OneShotCollectorTest, InitiallyEnabled) {
  settings_->SetBoolean(kEnableSettingPath, true);

  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));
  base::test::TestFuture<Status> test_future;

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false,
                             test_future.GetCallback());

  // Setting is initially enabled, data is being collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  settings_->SetBoolean(kEnableSettingPath, false);
  settings_->SetBoolean(kEnableSettingPath, true);

  // No more data should be collected even if the setting was disabled then
  // re-enabled.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);
  EXPECT_TRUE(test_future.Wait());

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_info_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(OneShotCollectorTest, NoMetricData) {
  settings_->SetBoolean(kEnableSettingPath, true);

  sampler_->SetMetricData(absl::nullopt);

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false);

  // Setting is initially enabled, data is being collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(OneShotCollectorTest, InitiallyDisabled) {
  settings_->SetBoolean(kEnableSettingPath, false);

  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false);

  // Setting is initially disabled, no data is collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 0);

  settings_->SetBoolean(kEnableSettingPath, true);

  // Setting is enabled, data is being collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  settings_->SetBoolean(kEnableSettingPath, false);
  settings_->SetBoolean(kEnableSettingPath, true);

  // No more data should be collected even if the setting was disabled then
  // re-enabled.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_info_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
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
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);
  EXPECT_TRUE(test_future.Wait());

  const auto& metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_info_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
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
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 0);
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}
}  // namespace
}  // namespace reporting
