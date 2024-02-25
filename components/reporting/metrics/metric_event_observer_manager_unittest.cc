// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_event_observer_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/fakes/fake_event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/fakes/fake_metric_event_observer.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;

namespace reporting {
namespace {

constexpr char kEventEnableSettingPath[] = "event_enable_path";
constexpr base::TimeDelta init_delay = base::Minutes(1);

class MockCollector : public CollectorBase {
 public:
  MockCollector() : CollectorBase(/*sampler=*/nullptr) {}

  MockCollector(const MockCollector& other) = delete;
  MockCollector& operator=(const MockCollector& other) = delete;

  ~MockCollector() override = default;

  MOCK_METHOD(void, Collect, (bool), (override));

 protected:
  MOCK_METHOD(void,
              OnMetricDataCollected,
              (bool, std::optional<MetricData>),
              (override));

  MOCK_METHOD(bool, CanCollect, (), (const override));
};

class MetricEventObserverManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    settings_ = std::make_unique<test::FakeReportingSettings>();
    event_observer_ = std::make_unique<test::FakeMetricEventObserver>();
    metric_report_queue_ = std::make_unique<test::FakeMetricReportQueue>();
    collector_pool_ =
        std::make_unique<test::FakeEventDrivenTelemetryCollectorPool>();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<test::FakeReportingSettings> settings_;
  std::unique_ptr<test::FakeMetricEventObserver> event_observer_;
  std::unique_ptr<test::FakeMetricReportQueue> metric_report_queue_;
  std::unique_ptr<test::FakeEventDrivenTelemetryCollectorPool> collector_pool_;
};

TEST_F(MetricEventObserverManagerTest, InitiallyEnabled) {
  settings_->SetReportingEnabled(kEventEnableSettingPath, true);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath, /*setting_enabled_default_value=*/false,
      /*collector_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  size_t reporting_count = 2;
  for (size_t i = 0; i < reporting_count; ++i) {
    event_observer_ptr->RunCallback(metric_data);

    MetricData metric_data_reported =
        metric_report_queue_->GetMetricDataReported();
    EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
    EXPECT_TRUE(metric_data_reported.has_event_data());
  }

  // Setting disabled, no more data should be reported even if the callback is
  // called.
  settings_->SetReportingEnabled(kEventEnableSettingPath, false);

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(MetricEventObserverManagerTest, InitiallyEnabled_Delayed) {
  settings_->SetReportingEnabled(kEventEnableSettingPath, true);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath, /*setting_enabled_default_value=*/true,
      /*sampler_pool=*/nullptr, init_delay);

  MetricData metric_data;
  metric_data.mutable_event_data();

  // `init_delay` not elapsed, reporting enabled value not registered.
  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());

  event_observer_ptr->RunCallback(metric_data);
  ASSERT_TRUE(metric_report_queue_->IsEmpty());

  task_environment_.FastForwardBy(init_delay);

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());

  event_observer_ptr->RunCallback(metric_data);
  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_event_data());

  // Setting disabled, no more data should be reported even if the callback is
  // called.
  settings_->SetReportingEnabled(kEventEnableSettingPath, false);

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(MetricEventObserverManagerTest, InitiallyDisabled_Delayed) {
  settings_->SetReportingEnabled(kEventEnableSettingPath, false);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath, /*setting_enabled_default_value=*/true,
      /*sampler_pool=*/nullptr, init_delay);

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());

  event_observer_ptr->RunCallback(metric_data);
  ASSERT_TRUE(metric_report_queue_->IsEmpty());

  settings_->SetReportingEnabled(kEventEnableSettingPath, true);

  task_environment_.FastForwardBy(init_delay / 2);

  // `init_delay` still not elapsed, reporting enabled value not registered.
  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());

  task_environment_.FastForwardBy(init_delay / 2);

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());

  event_observer_ptr->RunCallback(metric_data);
  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_event_data());
}

TEST_F(MetricEventObserverManagerTest, InitiallyDisabled) {
  settings_->SetReportingEnabled(kEventEnableSettingPath, false);
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath, /*setting_enabled_default_value=*/false,
      /*collector_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());

  settings_->SetReportingEnabled(kEventEnableSettingPath, true);

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_event_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(MetricEventObserverManagerTest, DefaultEnabled) {
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/true,
      /*collector_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());
  event_observer_ptr->RunCallback(metric_data);

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_event_data());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(MetricEventObserverManagerTest, DefaultDisabled) {
  auto* event_observer_ptr = event_observer_.get();

  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      "invalid/path", /*setting_enabled_default_value=*/false,
      /*collector_pool=*/nullptr);

  MetricData metric_data;
  metric_data.mutable_event_data();

  event_observer_ptr->RunCallback(metric_data);

  ASSERT_FALSE(event_observer_ptr->GetReportingEnabled());
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(MetricEventObserverManagerTest, EventDrivenTelemetry) {
  settings_->SetReportingEnabled(kEventEnableSettingPath, true);
  auto* event_observer_ptr = event_observer_.get();
  MetricEventType network_event = MetricEventType::WIFI_SIGNAL_STRENGTH_LOW;

  MockCollector network_collector1;
  MockCollector network_collector2;
  MockCollector usb_collector;
  collector_pool_->AddEventTelemetryCollector(network_event,
                                              &network_collector1);
  collector_pool_->AddEventTelemetryCollector(network_event,
                                              &network_collector2);
  collector_pool_->AddEventTelemetryCollector(MetricEventType::USB_ADDED,
                                              &usb_collector);
  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath,
      /*setting_enabled_default_value=*/false, collector_pool_.get());

  MetricData event_metric_data;
  event_metric_data.mutable_event_data()->set_type(network_event);

  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());

  EXPECT_CALL(network_collector1, Collect(_))
      .WillOnce([](bool is_event_driven) { ASSERT_TRUE(is_event_driven); });
  EXPECT_CALL(network_collector2, Collect(_))
      .WillOnce([](bool is_event_driven) { ASSERT_TRUE(is_event_driven); });
  EXPECT_CALL(usb_collector, Collect(_)).Times(0);

  event_observer_ptr->RunCallback(std::move(event_metric_data));
  task_environment_.RunUntilIdle();

  MetricData metric_data_reported =
      metric_report_queue_->GetMetricDataReported();

  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_event_data());
  EXPECT_THAT(metric_data_reported.event_data().type(), Eq(network_event));
  EXPECT_TRUE(metric_report_queue_->IsEmpty());
}

TEST_F(MetricEventObserverManagerTest, ReportsEnqueuedEventsToUMA) {
  settings_->SetReportingEnabled(kEventEnableSettingPath, true);

  auto* const event_observer_ptr = event_observer_.get();
  MetricEventObserverManager event_manager(
      std::move(event_observer_), metric_report_queue_.get(), settings_.get(),
      kEventEnableSettingPath, /*setting_enabled_default_value=*/false,
      /*collector_pool=*/nullptr);
  ASSERT_TRUE(event_observer_ptr->GetReportingEnabled());

  static constexpr MetricEventType kEventType = MetricEventType::FATAL_CRASH;
  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(kEventType);

  const base::HistogramTester histogram_tester;
  static constexpr size_t kTotalRecordCount = 2;
  for (size_t record_count = 1; record_count <= kTotalRecordCount;
       ++record_count) {
    event_observer_ptr->RunCallback(metric_data);
    const MetricData metric_data_reported =
        metric_report_queue_->GetMetricDataReported();
    EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
    ASSERT_TRUE(metric_data_reported.has_event_data());
    EXPECT_THAT(metric_data_reported.event_data().type(), Eq(kEventType));
    histogram_tester.ExpectBucketCount(
        MetricEventObserverManager::kEventMetricEnqueuedMetricsName, kEventType,
        record_count);
    histogram_tester.ExpectTotalCount(
        MetricEventObserverManager::kEventMetricEnqueuedMetricsName,
        record_count);
  }
}

}  // namespace
}  // namespace reporting
