// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_event_observer_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/metrics/fake_metric_report_queue.h"
#include "components/reporting/metrics/fake_reporting_settings.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

class MetricEventObserverManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    settings_ = std::make_unique<test::FakeReportingSettings>();
    event_observer_ = std::make_unique<test::FakeMetricEventObserver>();
    metric_report_queue_ = std::make_unique<test::FakeMetricReportQueue>();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  const std::string kEnableSettingPath = "enable_path";

  std::unique_ptr<test::FakeReportingSettings> settings_;
  std::unique_ptr<test::FakeMetricEventObserver> event_observer_;
  std::unique_ptr<test::FakeMetricReportQueue> metric_report_queue_;
};

TEST_F(MetricEventObserverManagerTest, InitiallyEnabled) {
  settings_->SetBoolean(kEnableSettingPath, true);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEnableSettingPath, /*setting_enabled_default_value=*/false);

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  size_t reporting_count = 2;
  for (size_t i = 0; i < reporting_count; ++i) {
    event_observer_ptr->RunCallback(metric_data);

    auto metric_data_reported = metric_report_queue_->GetMetricDataReported();
    ASSERT_EQ(metric_data_reported.size(), i + 1);
    EXPECT_TRUE(metric_data_reported[i].has_timestamp_ms());
    EXPECT_TRUE(metric_data_reported[i].has_event_data());
  }

  // Setting disabled, no more data should be reported even if the callback is
  // called.
  settings_->SetBoolean(kEnableSettingPath, false);

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_EQ(metric_report_queue_->GetMetricDataReported().size(),
            reporting_count);
}

TEST_F(MetricEventObserverManagerTest, InitiallyDisabled) {
  settings_->SetBoolean(kEnableSettingPath, false);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEnableSettingPath, /*setting_enabled_default_value=*/false);

  MetricData metric_data;
  metric_data.mutable_event_data();

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->GetMetricDataReported().empty());

  settings_->SetBoolean(kEnableSettingPath, true);

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();
  ASSERT_EQ(metric_data_reported.size(), 1ul);
  EXPECT_TRUE(metric_data_reported[0].has_event_data());
}

TEST_F(MetricEventObserverManagerTest, DefaultEnabled) {
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/true);

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  event_observer_ptr->RunCallback(metric_data);

  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();
  ASSERT_EQ(metric_data_reported.size(), 1ul);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0].has_event_data());
}

TEST_F(MetricEventObserverManagerTest, DefaultDisabled) {
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/false);

  MetricData metric_data;
  metric_data.mutable_event_data();

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->GetMetricDataReported().empty());
}

TEST_F(MetricEventObserverManagerTest, AdditionalSamplers) {
  settings_->SetBoolean(kEnableSettingPath, true);
  auto* event_observer_ptr = event_observer_.get();

  MetricData additional_metric_data;
  additional_metric_data.mutable_telemetry_data();

  test::FakeSampler additional_sampler;

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEnableSettingPath, /*setting_enabled_default_value=*/false,
      {&additional_sampler});

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  for (size_t i = 0; i < 2; ++i) {
    base::RunLoop run_loop;
    additional_sampler.SetMetricData(additional_metric_data);
    event_observer_ptr->RunCallback(metric_data);
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();

    auto metric_data_reported = metric_report_queue_->GetMetricDataReported();
    ASSERT_EQ(metric_data_reported.size(), i + 1);
    EXPECT_TRUE(metric_data_reported[i].has_timestamp_ms());
    EXPECT_TRUE(metric_data_reported[i].has_event_data());
    EXPECT_TRUE(metric_data_reported[i].has_telemetry_data());
  }
}

}  // namespace
}  // namespace reporting
