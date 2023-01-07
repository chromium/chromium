// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_event_observer_manager.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/metrics/fake_event_driven_telemetry_sampler_pool.h"
#include "components/reporting/metrics/fake_metric_report_queue.h"
#include "components/reporting/metrics/fake_reporting_settings.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace {

constexpr char kEventEnableSettingPath[] = "event_enable_path";

class MetricEventObserverManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    settings_ = std::make_unique<test::FakeReportingSettings>();
    event_observer_ = std::make_unique<test::FakeMetricEventObserver>();
    metric_report_queue_ = std::make_unique<test::FakeMetricReportQueue>();
    sampler_pool_ =
        std::make_unique<test::FakeEventDrivenTelemetrySamplerPool>();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<test::FakeReportingSettings> settings_;
  std::unique_ptr<test::FakeMetricEventObserver> event_observer_;
  std::unique_ptr<test::FakeMetricReportQueue> metric_report_queue_;
  std::unique_ptr<test::FakeEventDrivenTelemetrySamplerPool> sampler_pool_;
};

TEST_F(MetricEventObserverManagerTest, InitiallyEnabled) {
  settings_->SetBoolean(kEventEnableSettingPath, true);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath, /*setting_enabled_default_value=*/false,
      /*sampler_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  size_t reporting_count = 2;
  for (size_t i = 0; i < reporting_count; ++i) {
    event_observer_ptr->RunCallback(metric_data);

    const auto& metric_data_reported =
        metric_report_queue_->GetMetricDataReported();
    ASSERT_THAT(metric_data_reported, ::testing::SizeIs(i + 1));
    EXPECT_TRUE(metric_data_reported[i]->has_timestamp_ms());
    EXPECT_TRUE(metric_data_reported[i]->has_event_data());
  }

  // Setting disabled, no more data should be reported even if the callback is
  // called.
  settings_->SetBoolean(kEventEnableSettingPath, false);

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_THAT(metric_report_queue_->GetMetricDataReported(),
              ::testing::SizeIs(reporting_count));
}

TEST_F(MetricEventObserverManagerTest, InitiallyDisabled) {
  settings_->SetBoolean(kEventEnableSettingPath, false);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath, /*setting_enabled_default_value=*/false,
      /*sampler_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->GetMetricDataReported().empty());

  settings_->SetBoolean(kEventEnableSettingPath, true);

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  const auto& metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  ASSERT_THAT(metric_data_reported, ::testing::SizeIs(1));
  EXPECT_TRUE(metric_data_reported[0]->has_event_data());
}

TEST_F(MetricEventObserverManagerTest, DefaultEnabled) {
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/true,
      /*sampler_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  event_observer_ptr->RunCallback(metric_data);

  const auto& metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  ASSERT_THAT(metric_data_reported, ::testing::SizeIs(1));
  EXPECT_TRUE(metric_data_reported[0]->has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0]->has_event_data());
}

TEST_F(MetricEventObserverManagerTest, DefaultDisabled) {
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/false,
      /*sampler_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->GetMetricDataReported().empty());
}

TEST_F(MetricEventObserverManagerTest, EventDrivenTelemetry) {
  settings_->SetBoolean(kEventEnableSettingPath, true);
  auto* event_observer_ptr = event_observer_.get();

  std::vector<std::string> telemetry_paths = {"path1", "path2", "path3"};
  std::vector<std::unique_ptr<ConfiguredSampler>> configured_samplers;
  MetricData telemetry_data[3];

  telemetry_data[0].mutable_telemetry_data()->mutable_audio_telemetry();
  telemetry_data[1].mutable_telemetry_data()->mutable_peripherals_telemetry();
  telemetry_data[2].mutable_telemetry_data()->mutable_networks_telemetry();

  for (size_t i = 0; i < telemetry_paths.size(); ++i) {
    auto sampler = std::make_unique<test::FakeSampler>();
    sampler->SetMetricData(std::move(telemetry_data[i]));
    configured_samplers.push_back(std::make_unique<ConfiguredSampler>(
        std::move(sampler), telemetry_paths[i],
        /*setting_enabled_default_value=*/false, settings_.get()));
    sampler_pool_->AddEventSampler(
        MetricEventType::NETWORK_CONNECTION_STATE_CHANGE,
        configured_samplers.at(i).get());
  }
  settings_->SetBoolean(telemetry_paths[0], true);
  // Disable second sampler collection.
  settings_->SetBoolean(telemetry_paths[1], false);
  settings_->SetBoolean(telemetry_paths[2], true);

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath,
      /*setting_enabled_default_value=*/false, sampler_pool_.get());

  MetricData event_metric_data;
  event_metric_data.mutable_event_data()->set_type(
      MetricEventType::NETWORK_CONNECTION_STATE_CHANGE);

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());

  event_observer_ptr->RunCallback(std::move(event_metric_data));
  task_environment_.RunUntilIdle();

  const auto& metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  ASSERT_THAT(metric_data_reported, ::testing::SizeIs(1));
  EXPECT_TRUE(metric_data_reported[0]->has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0]->has_event_data());
  ASSERT_TRUE(metric_data_reported[0]->has_telemetry_data());
  EXPECT_TRUE(metric_data_reported[0]->telemetry_data().has_audio_telemetry());
  EXPECT_FALSE(
      metric_data_reported[0]->telemetry_data().has_peripherals_telemetry());
  EXPECT_TRUE(
      metric_data_reported[0]->telemetry_data().has_networks_telemetry());
}

}  // namespace
}  // namespace reporting
