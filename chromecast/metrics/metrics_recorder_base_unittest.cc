// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/metrics_recorder_base.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/metrics/mock_cast_event_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace chromecast {

namespace {

class MockMetricsRecorder : public MetricsRecorderBase {
 public:
  explicit MockMetricsRecorder(const base::TickClock* tick_clock)
      : MetricsRecorderBase(tick_clock) {}
  ~MockMetricsRecorder() override;

  std::unique_ptr<CastEventBuilder> CreateEventBuilder(
      const std::string& name) override {
    auto builder = std::make_unique<FakeCastEventBuilder>();
    builder->SetName(name);
    return builder;
  }

  MOCK_METHOD(void,
              AddActiveConnection,
              (const std::string&,
               const std::string&,
               const base::Value&,
               const net::IPAddressBytes&),
              (override));
  MOCK_METHOD(void, RemoveActiveConnection, (const std::string&), (override));
  MOCK_METHOD(void,
              RecordCastEvent,
              (std::unique_ptr<CastEventBuilder> event_builder),
              (override));
  MOCK_METHOD(void,
              RecordHistogramTime,
              (const std::string&, int, int, int, int),
              (override));
  MOCK_METHOD(void,
              RecordHistogramCount,
              (const std::string&, int, int, int, int),
              (override));
  MOCK_METHOD(void,
              RecordHistogramCountRepeated,
              (const std::string&, int, int, int, int, int),
              (override));
  MOCK_METHOD(void,
              RecordHistogramEnum,
              (const std::string&, int, int),
              (override));
  MOCK_METHOD(void,
              RecordHistogramSparse,
              (const std::string&, int),
              (override));
};

inline MockMetricsRecorder::~MockMetricsRecorder() = default;

MATCHER_P2(NameAndExtraValue, name, extra_value, "") {
  auto* builder = static_cast<FakeCastEventBuilder*>(arg.get());
  if (!builder)
    return false;
  return builder->name == name && builder->extra_value == extra_value;
}

}  // namespace

class MetricsRecorderTest : public ::testing::Test {
 public:
  MetricsRecorderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        recorder_(task_environment_.GetMockTickClock()) {}

  base::test::TaskEnvironment task_environment_;
  MockMetricsRecorder recorder_;
};

TEST_F(MetricsRecorderTest, TimelineIntervals) {
  // Demonstrates usage of the timeline measurement functions.
  recorder_.MeasureTimeUntilEvent("MyEvent", "MyEvent.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(1500));

  EXPECT_CALL(recorder_,
              RecordCastEvent(NameAndExtraValue("MyEvent.LatencyMs", 1500)));
  recorder_.RecordTimelineEvent("MyEvent");
}

TEST_F(MetricsRecorderTest, TimelineIntervalsMultipleMeasurement) {
  // Multiple measurements with the same end event will each trigger a
  // measurement metric event.
  recorder_.MeasureTimeUntilEvent("MyEvent", "First.Measurement.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(300));
  recorder_.MeasureTimeUntilEvent("MyEvent", "Second.Measurement.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(800));

  EXPECT_CALL(recorder_, RecordCastEvent(NameAndExtraValue(
                             "First.Measurement.LatencyMs", 1100)));
  EXPECT_CALL(recorder_, RecordCastEvent(NameAndExtraValue(
                             "Second.Measurement.LatencyMs", 800)));
  recorder_.RecordTimelineEvent("MyEvent");
}

TEST_F(MetricsRecorderTest, TimelineIntervalsDuplicateMeasurement) {
  // Measurements can have the same name and/or end event. Each instance will
  // emit a metric event containing the measurement value.
  recorder_.MeasureTimeUntilEvent("AnEvent", "AnEvent.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(600));
  recorder_.MeasureTimeUntilEvent("AnEvent", "AnEvent.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(700));

  EXPECT_CALL(recorder_,
              RecordCastEvent(NameAndExtraValue("AnEvent.LatencyMs", 1300)));
  EXPECT_CALL(recorder_,
              RecordCastEvent(NameAndExtraValue("AnEvent.LatencyMs", 700)));
  recorder_.RecordTimelineEvent("AnEvent");
}

TEST_F(MetricsRecorderTest, TimelineIntervalsRepeatEnd) {
  // The measurement should be cleared when the ending event is triggered.
  recorder_.MeasureTimeUntilEvent("SomeEvent", "SomeEvent.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(2300));

  EXPECT_CALL(recorder_,
              RecordCastEvent(NameAndExtraValue("SomeEvent.LatencyMs", 2300)));
  recorder_.RecordTimelineEvent("SomeEvent");

  // Repeating the end event won't trigger another latency measurement event.
  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_CALL(recorder_, RecordCastEvent(_)).Times(0);
  recorder_.RecordTimelineEvent("SomeEvent");
}

TEST_F(MetricsRecorderTest, TimelineIntervalsRepeatMonitoring) {
  // Measurements can be repeated after they have finished.
  recorder_.MeasureTimeUntilEvent("AnEvent", "AnEvent.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(1100));

  EXPECT_CALL(recorder_,
              RecordCastEvent(NameAndExtraValue("AnEvent.LatencyMs", 1100)));
  recorder_.RecordTimelineEvent("AnEvent");

  // Monitoring again should eventually generate another latency event if both
  // monitored events are triggered in order.
  recorder_.MeasureTimeUntilEvent("AnEvent", "AnEvent.LatencyMs");
  task_environment_.FastForwardBy(base::Milliseconds(500));

  EXPECT_CALL(recorder_,
              RecordCastEvent(NameAndExtraValue("AnEvent.LatencyMs", 500)));
  recorder_.RecordTimelineEvent("AnEvent");
}

}  // namespace chromecast
