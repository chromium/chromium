// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/available_memory_monitor_evaluator.h"

#include <memory>
#include <utility>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

class TestAvailableMemoryMonitorEvaluator
    : public AvailableMemoryMonitorEvaluator {
 public:
  explicit TestAvailableMemoryMonitorEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : AvailableMemoryMonitorEvaluator(std::move(voter)) {
    StopObserving();
  }

  ~TestAvailableMemoryMonitorEvaluator() override = default;

  using AvailableMemoryMonitorEvaluator::CheckMemoryPressure;

  void set_next_pressure_level(base::MemoryPressureLevel level) {
    next_pressure_level_ = level;
  }

 protected:
  base::MemoryPressureLevel CalculateCurrentPressureLevel(
      const base::AvailableMemoryMonitor::MemorySample& sample) override {
    return next_pressure_level_;
  }

 private:
  base::MemoryPressureLevel next_pressure_level_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;
};

}  // namespace

class AvailableMemoryMonitorEvaluatorTest : public testing::Test {
 public:
  AvailableMemoryMonitorEvaluatorTest() = default;
  ~AvailableMemoryMonitorEvaluatorTest() override = default;

 protected:
  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AvailableMemoryMonitorEvaluatorTest, CheckMemoryPressure) {
  MultiSourceMemoryPressureMonitor monitor;
  TestAvailableMemoryMonitorEvaluator evaluator(monitor.CreateVoter());

  testing::StrictMock<base::RegisteredMockMemoryPressureListener> listener;

  base::AvailableMemoryMonitor::MemorySample sample;
  sample.timestamp = base::TimeTicks::Now();

  // Initial check at NONE should not notify.
  evaluator.set_next_pressure_level(base::MEMORY_PRESSURE_LEVEL_NONE);
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());

  // Transition NONE -> MODERATE notifies.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.set_next_pressure_level(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Sustained MODERATE during cooldown: does not re-notify before
  // kModeratePressureCooldown elapses, then notifies once it elapses.
  task_environment_.FastForwardBy(
      MemoryPressureVoteAggregator::kModeratePressureCooldown -
      base::Seconds(1));
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  task_environment_.FastForwardBy(base::Seconds(1));
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Transition MODERATE -> CRITICAL always notifies.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.set_next_pressure_level(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment_.FastForwardBy(base::Seconds(2));
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Sustained CRITICAL during cooldown: does not re-notify before
  // kCriticalPressureCooldown elapses, then notifies once it elapses.
  task_environment_.FastForwardBy(
      MemoryPressureVoteAggregator::kCriticalPressureCooldown -
      base::Seconds(1));
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  task_environment_.FastForwardBy(base::Seconds(1));
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Transition CRITICAL -> MODERATE notifies.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.set_next_pressure_level(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment_.FastForwardBy(base::Seconds(2));
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Transition MODERATE -> NONE notifies.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE));
  evaluator.set_next_pressure_level(base::MEMORY_PRESSURE_LEVEL_NONE);
  task_environment_.FastForwardBy(base::Seconds(2));
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Sustained NONE does not notify.
  task_environment_.FastForwardBy(
      MemoryPressureVoteAggregator::kModeratePressureCooldown);
  sample.timestamp = base::TimeTicks::Now();
  evaluator.CheckMemoryPressure(sample);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);
}

}  // namespace memory_pressure
