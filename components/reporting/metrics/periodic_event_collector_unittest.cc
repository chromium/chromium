// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/periodic_event_collector.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using reporting::test::FakeReportingSettings;
using reporting::test::FakeSampler;
using testing::Eq;

namespace reporting {
namespace {

constexpr char kRateSettingPath[] = "rate_path";

class FakeEventDetector : public PeriodicEventCollector::EventDetector {
 public:
  FakeEventDetector() = default;

  FakeEventDetector(const FakeEventDetector& other) = delete;
  FakeEventDetector& operator=(const FakeEventDetector& other) = delete;

  ~FakeEventDetector() override = default;

  std::optional<MetricEventType> DetectEvent(
      std::optional<MetricData> previous_metric_data,
      const MetricData& current_metric_data) override {
    previous_metric_data_ = previous_metric_data;
    return event_type_;
  }

  void SetEventType(std::optional<MetricEventType> event_type) {
    event_type_ = event_type;
  }

  std::optional<MetricData> GetPreviousMetricData() const {
    return previous_metric_data_;
  }

 private:
  std::optional<MetricData> previous_metric_data_;

  std::optional<MetricEventType> event_type_;
};

class PeriodicEventCollectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    settings_ = std::make_unique<FakeReportingSettings>();
    sampler_ = std::make_unique<FakeSampler>();
    event_detector_ = std::make_unique<FakeEventDetector>();
    event_detector_ptr_ = event_detector_.get();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<FakeReportingSettings> settings_;
  std::unique_ptr<FakeSampler> sampler_;
  std::unique_ptr<FakeEventDetector> event_detector_;
  raw_ptr<FakeEventDetector, DanglingUntriaged> event_detector_ptr_;
};

TEST_F(PeriodicEventCollectorTest, Default) {
  constexpr int interval = 10000;
  bool event_observed_called = false;
  MetricData event_metric_data;
  int expected_collections = 0;
  MetricEventType event_type = MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE;

  settings_->SetInteger(kRateSettingPath, interval);
  MetricData sampler_data;
  sampler_data.mutable_telemetry_data()->mutable_networks_telemetry();

  PeriodicEventCollector periodic_event_collector(
      sampler_.get(), std::move(event_detector_), settings_.get(),
      kRateSettingPath, /*default_rate=*/base::Minutes(10));
  periodic_event_collector.SetOnEventObservedCallback(
      base::BindLambdaForTesting([&](MetricData metric_data) {
        event_observed_called = true;
        event_metric_data = std::move(metric_data);
      }));
  event_detector_ptr_->SetEventType(event_type);
  sampler_->SetMetricData(sampler_data);

  {
    // task_environment_.FastForwardBy(base::Milliseconds(interval));
    base::RunLoop().RunUntilIdle();

    // Reporting enabled not set, sampler data is not collected and no events
    // are observed.
    EXPECT_THAT(sampler_->GetNumCollectCalls(),
                testing::Eq(expected_collections));
    EXPECT_FALSE(event_observed_called);
  }

  {
    // Setting reporting enabled to initially to false should not cause
    // event collection.
    periodic_event_collector.SetReportingEnabled(false);
    task_environment_.FastForwardBy(base::Milliseconds(interval));
    base::RunLoop().RunUntilIdle();

    // Reporting enabled not set, sampler data is not collected and no events
    // are observed.
    EXPECT_THAT(sampler_->GetNumCollectCalls(),
                testing::Eq(expected_collections));
    EXPECT_FALSE(event_observed_called);
  }

  {
    // Reporting enabled, one initial collection should take place.
    periodic_event_collector.SetReportingEnabled(true);
    ++expected_collections;
    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collections));
    EXPECT_FALSE(event_detector_ptr_->GetPreviousMetricData().has_value());
    EXPECT_TRUE(event_observed_called);
    EXPECT_THAT(event_metric_data.event_data().type(), Eq(event_type));
    EXPECT_TRUE(event_metric_data.has_timestamp_ms());
    EXPECT_TRUE(event_metric_data.has_telemetry_data());
    EXPECT_TRUE(event_metric_data.telemetry_data().has_networks_telemetry());
  }

  {
    event_observed_called = false;
    sampler_data.Clear();
    sampler_data.mutable_telemetry_data()->mutable_audio_telemetry();
    sampler_->SetMetricData(sampler_data);

    // Only forward time by half of the collection interval.
    task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
    base::RunLoop().RunUntilIdle();

    // Reporting enabled but time not elapsed, sampler data is not collected and
    // no events are observed.
    EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collections));
    EXPECT_FALSE(event_observed_called);

    // Forward time by the remaining half of the collection interval.
    task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
    base::RunLoop().RunUntilIdle();

    ++expected_collections;
    EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collections));
    // Assert previously collected data.
    ASSERT_TRUE(event_detector_ptr_->GetPreviousMetricData().has_value());
    MetricData previous_metric_data =
        event_detector_ptr_->GetPreviousMetricData().value();
    EXPECT_THAT(previous_metric_data.event_data().type(), Eq(event_type));
    EXPECT_TRUE(previous_metric_data.has_telemetry_data());
    EXPECT_TRUE(previous_metric_data.telemetry_data().has_networks_telemetry());
    // Assert current collected data.
    EXPECT_TRUE(event_observed_called);
    EXPECT_THAT(event_metric_data.event_data().type(), Eq(event_type));
    EXPECT_TRUE(event_metric_data.has_timestamp_ms());
    EXPECT_TRUE(event_metric_data.has_telemetry_data());
    EXPECT_TRUE(event_metric_data.telemetry_data().has_audio_telemetry());
  }

  {
    event_observed_called = false;
    event_detector_ptr_->SetEventType(std::nullopt);
    sampler_data.Clear();
    sampler_data.mutable_telemetry_data()->mutable_app_telemetry();
    sampler_->SetMetricData(sampler_data);

    // Forward time by the the collection interval.
    task_environment_.FastForwardBy(base::Milliseconds(interval));
    base::RunLoop().RunUntilIdle();

    ++expected_collections;
    EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collections));
    ASSERT_TRUE(event_detector_ptr_->GetPreviousMetricData().has_value());
    MetricData previous_metric_data =
        event_detector_ptr_->GetPreviousMetricData().value();
    EXPECT_THAT(previous_metric_data.event_data().type(), Eq(event_type));
    EXPECT_TRUE(previous_metric_data.has_telemetry_data());
    EXPECT_TRUE(previous_metric_data.telemetry_data().has_audio_telemetry());
    // Data collected but no event detected.
    EXPECT_FALSE(event_observed_called);
  }

  {
    periodic_event_collector.SetReportingEnabled(false);

    // Forward time by the the collection interval.
    task_environment_.FastForwardBy(base::Milliseconds(interval));
    base::RunLoop().RunUntilIdle();

    // Number of collections is not incremented, no new collections since
    // reporting is disabled.
    EXPECT_THAT(sampler_->GetNumCollectCalls(), Eq(expected_collections));
    EXPECT_FALSE(event_observed_called);
  }
}
}  // namespace
}  // namespace reporting
