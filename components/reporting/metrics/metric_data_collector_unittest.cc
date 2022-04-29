// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_data_collector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/fake_metric_report_queue.h"
#include "components/reporting/metrics/fake_reporting_settings.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace test {

class FakeEventDetector : public EventDetector {
 public:
  FakeEventDetector() = default;

  FakeEventDetector(const FakeEventDetector& other) = delete;
  FakeEventDetector& operator=(const FakeEventDetector& other) = delete;

  ~FakeEventDetector() override = default;

  absl::optional<MetricEventType> DetectEvent(
      const MetricData& previous_metric_data,
      const MetricData& current_metric_data) override {
    previous_metric_list_.emplace_back(previous_metric_data);
    if (!has_event_) {
      return absl::nullopt;
    }
    return MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE;
  }

  void SetHasEvent(bool has_event) { has_event_ = has_event; }

  std::vector<MetricData> GetPreviousMetricList() {
    return previous_metric_list_;
  }

 private:
  bool has_event_ = false;

  std::vector<MetricData> previous_metric_list_;
};
}  // namespace test

namespace {

class MetricDataCollectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    settings_ = std::make_unique<test::FakeReportingSettings>();
    sampler_ = std::make_unique<test::FakeSampler>();
    metric_report_queue_ = std::make_unique<test::FakeMetricReportQueue>();
  }

  void FlushTasks() {
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const std::string kEnableSettingPath = "enable_path";
  const std::string kRateSettingPath = "rate_path";

  std::unique_ptr<test::FakeReportingSettings> settings_;
  std::unique_ptr<test::FakeSampler> sampler_;
  std::unique_ptr<test::FakeMetricReportQueue> metric_report_queue_;
};

TEST_F(MetricDataCollectorTest, OneShotCollector_InitiallyEnabled) {
  settings_->SetBoolean(kEnableSettingPath, true);

  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));
  bool callback_called = false;

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false,
                             base::BindLambdaForTesting([&callback_called]() {
                               callback_called = true;
                             }));

  // Setting is initially enabled, data is being collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  settings_->SetBoolean(kEnableSettingPath, false);
  settings_->SetBoolean(kEnableSettingPath, true);

  // No more data should be collected even if the setting was disabled then
  // re-enabled.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 1ul);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0].has_info_data());
}

TEST_F(MetricDataCollectorTest, OneShotCollector_NoMetricData) {
  settings_->SetBoolean(kEnableSettingPath, true);

  sampler_->SetMetricData(absl::nullopt);
  bool callback_called = false;

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false,
                             base::BindLambdaForTesting([&callback_called]() {
                               callback_called = true;
                             }));

  // Setting is initially enabled, data is being collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);
  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_TRUE(metric_data_reported.empty());
  EXPECT_FALSE(callback_called);
}

TEST_F(MetricDataCollectorTest, OneShotCollector_InitiallyDisabled) {
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

  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 1ul);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0].has_info_data());
}

TEST_F(MetricDataCollectorTest, OneShotCollector_DefaultEnabled) {
  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));
  bool callback_called = false;

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), "invalid/path",
                             /*setting_enabled_default_value=*/true,
                             base::BindLambdaForTesting([&callback_called]() {
                               callback_called = true;
                             }));

  // Setting is enabled by default, data is being collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 1ul);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0].has_info_data());
}

TEST_F(MetricDataCollectorTest, OneShotCollector_DefaultDisabled) {
  MetricData metric_data;
  metric_data.mutable_info_data();
  sampler_->SetMetricData(std::move(metric_data));

  OneShotCollector collector(sampler_.get(), metric_report_queue_.get(),
                             settings_.get(), kEnableSettingPath,
                             /*setting_enabled_default_value=*/false);
  FlushTasks();

  // Setting is disabled by default, no data is collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 0);
  EXPECT_TRUE(metric_report_queue_->GetMetricDataReported().empty());
}

TEST_F(MetricDataCollectorTest, PeriodicCollector_InitiallyEnabled) {
  constexpr int interval = 10000;
  settings_->SetBoolean(kEnableSettingPath, true);
  settings_->SetInteger(kRateSettingPath, interval);

  MetricData metric_data[5];
  metric_data[0].mutable_telemetry_data();
  metric_data[1].mutable_info_data();
  metric_data[2].mutable_event_data();
  metric_data[3].mutable_telemetry_data();
  metric_data[3].mutable_event_data();
  metric_data[4].mutable_info_data();
  metric_data[4].mutable_event_data();

  sampler_->SetMetricData(metric_data[0]);
  PeriodicCollector collector(
      sampler_.get(), metric_report_queue_.get(), settings_.get(),
      kEnableSettingPath, /*setting_enabled_default_value=*/false,
      kRateSettingPath, base::Milliseconds(interval / 2));

  // One initial collection at startup.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);
  FlushTasks();

  // Expected calls count initialized to 1 to reflect the initial collection.
  int expected_collect_calls = 1;
  for (int i = 0; i < 2; ++i) {
    sampler_->SetMetricData(metric_data[i + 1]);
    // 5 secs elapsed, no new data collected.
    task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
    EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);

    ++expected_collect_calls;
    // 10 secs elapsed, data should be collected.
    task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
    EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);
    FlushTasks();
  }

  sampler_->SetMetricData(metric_data[3]);
  settings_->SetBoolean(kEnableSettingPath, false);
  // Setting disabled, no data should be collected.
  task_environment_.FastForwardBy(base::Milliseconds(interval));
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);
  FlushTasks();

  settings_->SetBoolean(kEnableSettingPath, true);
  // Initial collection at policy enablement.
  ++expected_collect_calls;
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);
  FlushTasks();

  sampler_->SetMetricData(metric_data[4]);
  // Setting enabled, data should be collected after interval.
  task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);
  ++expected_collect_calls;
  task_environment_.FastForwardBy(base::Milliseconds(interval / 2));
  EXPECT_EQ(sampler_->GetNumCollectCalls(), expected_collect_calls);

  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 5ul);
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(metric_data_reported[i].has_timestamp_ms());
    EXPECT_EQ(metric_data_reported[i].has_telemetry_data(),
              metric_data[i].has_telemetry_data());
    EXPECT_EQ(metric_data_reported[i].has_info_data(),
              metric_data[i].has_info_data());
    EXPECT_EQ(metric_data_reported[i].has_event_data(),
              metric_data[i].has_event_data());
  }
}

TEST_F(MetricDataCollectorTest, PeriodicCollector_NoMetricData) {
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
  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_TRUE(metric_data_reported.empty());
}

TEST_F(MetricDataCollectorTest, PeriodicCollector_InitiallyDisabled) {
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
  FlushTasks();

  task_environment_.FastForwardBy(base::Milliseconds(interval));
  // 1 collection at policy enablement + 1 collection after interval.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 2);

  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 2ul);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0].has_telemetry_data());
  EXPECT_TRUE(metric_data_reported[1].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[1].has_telemetry_data());
}

TEST_F(MetricDataCollectorTest, PeriodicCollector_DefaultEnabled) {
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
  FlushTasks();

  metric_data.Clear();
  metric_data.mutable_event_data();
  sampler_->SetMetricData(std::move(metric_data));
  // 10 secs elapsed, data should be collected.
  task_environment_.FastForwardBy(base::Milliseconds(interval));
  // 1 collection at startup + 1 collection after interval.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 2);

  FlushTasks();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 2ul);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0].has_telemetry_data());
  EXPECT_TRUE(metric_data_reported[1].has_timestamp_ms());
  EXPECT_FALSE(metric_data_reported[1].has_telemetry_data());
  EXPECT_TRUE(metric_data_reported[1].has_event_data());
}

TEST_F(MetricDataCollectorTest, PeriodicCollector_DefaultDisabled) {
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
  FlushTasks();

  // Setting is disabled by default, no data collected.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 0);
  EXPECT_TRUE(metric_report_queue_->GetMetricDataReported().empty());
}

TEST_F(MetricDataCollectorTest, PeriodicEventCollector_NoAdditionalSamplers) {
  constexpr int interval = 10000;
  settings_->SetBoolean(kEnableSettingPath, true);
  settings_->SetInteger(kRateSettingPath, interval);

  MetricData metric_data[3];
  metric_data[0].mutable_info_data();
  metric_data[1].mutable_telemetry_data();
  metric_data[2].mutable_info_data();

  auto event_detector = std::make_unique<test::FakeEventDetector>();
  auto* event_detector_ptr = event_detector.get();

  sampler_->SetMetricData(metric_data[0]);
  PeriodicEventCollector collector(sampler_.get(), std::move(event_detector),
                                   {}, metric_report_queue_.get(),
                                   settings_.get(), kEnableSettingPath,
                                   /*setting_enabled_default_value=*/false,
                                   kRateSettingPath, base::Milliseconds(15000));

  // One initial collection at startup, data collected but not reported.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);
  FlushTasks();

  sampler_->SetMetricData(std::move(metric_data[1]));
  event_detector_ptr->SetHasEvent(true);
  task_environment_.FastForwardBy(base::Milliseconds(interval));
  // Data collected and reported.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 2);
  FlushTasks();

  sampler_->SetMetricData(std::move(metric_data[2]));
  event_detector_ptr->SetHasEvent(false);
  task_environment_.FastForwardBy(base::Milliseconds(interval));
  // Data collected but not reported.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 3);
  FlushTasks();
  auto previous_metric_list = event_detector_ptr->GetPreviousMetricList();

  ASSERT_EQ(previous_metric_list.size(), 3ul);

  EXPECT_FALSE(previous_metric_list[0].has_timestamp_ms());
  EXPECT_FALSE(previous_metric_list[0].has_info_data());
  EXPECT_FALSE(previous_metric_list[0].has_telemetry_data());
  EXPECT_FALSE(previous_metric_list[0].has_event_data());

  EXPECT_TRUE(previous_metric_list[1].has_timestamp_ms());
  EXPECT_TRUE(previous_metric_list[1].has_info_data());
  EXPECT_FALSE(previous_metric_list[1].has_telemetry_data());
  EXPECT_FALSE(previous_metric_list[1].has_event_data());

  EXPECT_TRUE(previous_metric_list[2].has_timestamp_ms());
  EXPECT_FALSE(previous_metric_list[2].has_info_data());
  EXPECT_TRUE(previous_metric_list[2].has_telemetry_data());
  EXPECT_TRUE(previous_metric_list[2].has_event_data());

  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 1ul);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_FALSE(metric_data_reported[0].has_info_data());
  EXPECT_TRUE(metric_data_reported[0].has_telemetry_data());
  EXPECT_TRUE(metric_data_reported[0].has_event_data());
}

TEST_F(MetricDataCollectorTest, PeriodicEventCollector_WithAdditionalSamplers) {
  settings_->SetBoolean(kEnableSettingPath, true);

  MetricData metric_data;
  metric_data.mutable_telemetry_data();

  MetricData additional_metric_data[3];
  additional_metric_data[0]
      .mutable_telemetry_data()
      ->mutable_networks_telemetry()
      ->mutable_https_latency_data()
      ->set_verdict(RoutineVerdict::PROBLEM);
  additional_metric_data[1]
      .mutable_telemetry_data()
      ->mutable_networks_telemetry()
      ->mutable_https_latency_data()
      ->set_problem(HttpsLatencyProblem::HIGH_LATENCY);
  additional_metric_data[2]
      .mutable_telemetry_data()
      ->mutable_networks_telemetry()
      ->mutable_https_latency_data()
      ->set_latency_ms(1500);

  test::FakeSampler additional_samplers[3];
  std::vector<Sampler*> additional_sampler_ptrs;
  for (int i = 0; i < 3; ++i) {
    additional_samplers[i].SetMetricData(additional_metric_data[i]);
    additional_sampler_ptrs.emplace_back(additional_samplers + i);
  }
  test::FakeSampler empty_additional_sampler;
  additional_sampler_ptrs.emplace_back(&empty_additional_sampler);

  auto event_detector = std::make_unique<test::FakeEventDetector>();

  sampler_->SetMetricData(std::move(metric_data));
  event_detector->SetHasEvent(true);
  PeriodicEventCollector collector(sampler_.get(), std::move(event_detector),
                                   std::move(additional_sampler_ptrs),
                                   metric_report_queue_.get(), settings_.get(),
                                   kEnableSettingPath,
                                   /*setting_enabled_default_value=*/false,
                                   kRateSettingPath, base::Milliseconds(15000));

  // Data collected and reported.
  EXPECT_EQ(sampler_->GetNumCollectCalls(), 1);

  task_environment_.RunUntilIdle();
  auto metric_data_reported = metric_report_queue_->GetMetricDataReported();

  ASSERT_EQ(metric_data_reported.size(), 1ul);
  EXPECT_TRUE(metric_data_reported[0].has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported[0].has_event_data());
  ASSERT_TRUE(metric_data_reported[0].has_telemetry_data());
  ASSERT_TRUE(
      metric_data_reported[0].telemetry_data().has_networks_telemetry());
  ASSERT_TRUE(metric_data_reported[0]
                  .telemetry_data()
                  .networks_telemetry()
                  .has_https_latency_data());

  auto https_latency_data = metric_data_reported[0]
                                .telemetry_data()
                                .networks_telemetry()
                                .https_latency_data();
  EXPECT_EQ(https_latency_data.verdict(), additional_metric_data[0]
                                              .telemetry_data()
                                              .networks_telemetry()
                                              .https_latency_data()
                                              .verdict());
  EXPECT_EQ(https_latency_data.problem(), additional_metric_data[1]
                                              .telemetry_data()
                                              .networks_telemetry()
                                              .https_latency_data()
                                              .problem());
  EXPECT_EQ(https_latency_data.latency_ms(), additional_metric_data[2]
                                                 .telemetry_data()
                                                 .networks_telemetry()
                                                 .https_latency_data()
                                                 .latency_ms());
}
}  // namespace
}  // namespace reporting
