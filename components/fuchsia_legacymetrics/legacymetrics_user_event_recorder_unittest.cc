// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_legacymetrics/legacymetrics_user_event_recorder.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia_legacymetrics {
namespace {

class LegacyMetricsUserActionRecorderTest : public testing::Test {
 public:
  LegacyMetricsUserActionRecorderTest() = default;
  ~LegacyMetricsUserActionRecorderTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(LegacyMetricsUserActionRecorderTest, ProduceAndConsume) {
  constexpr char kExpectedUserAction1[] = "Hello";
  constexpr char kExpectedUserAction2[] = "There";

  zx_time_t time_start = base::TimeTicks::Now().ToZxTime();
  LegacyMetricsUserActionRecorder buffer;
  base::RecordComputedAction(kExpectedUserAction1);
  EXPECT_TRUE(buffer.HasEvents());
  base::RecordComputedAction(kExpectedUserAction2);

  auto events = buffer.TakeEvents();
  EXPECT_FALSE(buffer.HasEvents());
  EXPECT_EQ(2u, events.size());

  // Verify the contents of the buffer are as expected.
  EXPECT_EQ(kExpectedUserAction1, events[0].name());
  EXPECT_GE(events[0].time(), time_start);
  EXPECT_EQ(kExpectedUserAction2, events[1].name());
  EXPECT_GE(events[1].time(), time_start);
  EXPECT_GE(events[1].time(), events[0].time());

  // Verify that the buffer is now empty.
  EXPECT_TRUE(buffer.TakeEvents().empty());

  // Add more data to the buffer, and then verify it, to ensure that recording
  // continues to work.
  base::RecordComputedAction(kExpectedUserAction2);
  EXPECT_TRUE(buffer.HasEvents());
  events = buffer.TakeEvents();
  EXPECT_FALSE(buffer.HasEvents());
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(kExpectedUserAction2, events[0].name());
}

TEST_F(LegacyMetricsUserActionRecorderTest, RecorderDeleted) {
  auto buffer = std::make_unique<LegacyMetricsUserActionRecorder>();
  buffer.reset();

  // |buffer| was destroyed, so check that recording actions doesn't cause a
  // use-after-free error.
  base::RecordComputedAction("NoCrashingPlz");
}

TEST_F(LegacyMetricsUserActionRecorderTest, EmptyBuffer) {
  LegacyMetricsUserActionRecorder buffer;
  EXPECT_FALSE(buffer.HasEvents());
}

TEST_F(LegacyMetricsUserActionRecorderTest, EnforcesMaximumEventCount) {
  LegacyMetricsUserActionRecorder buffer;

  // Try recording twice the maximum number of locally stored events.
  for (int i = 0, count = LegacyMetricsUserActionRecorder::kMaxEventCount * 2;
       i < count; i++) {
    base::RecordComputedAction("Test");
  }

  EXPECT_TRUE(buffer.HasEvents());
  auto events = buffer.TakeEvents();
  EXPECT_EQ(LegacyMetricsUserActionRecorder::kMaxEventCount, events.size());
}

}  // namespace
}  // namespace fuchsia_legacymetrics
