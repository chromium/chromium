// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/pressure/system_memory_pressure_evaluator.h"

#include <unistd.h>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {

namespace {

using PressureLevel = ResourcedClient::PressureLevel;

// Processes PressureCallback calls by just storing the sequence of events so we
// can validate that we received the expected pressure levels as the test runs.
void PressureCallback(
    std::vector<base::MemoryPressureListener::MemoryPressureLevel>* history,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  history->push_back(level);
}

}  // namespace

class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  TestSystemMemoryPressureEvaluator(
      bool for_testing,
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(for_testing, std::move(voter)) {}

  void OnMemoryPressure(
      PressureLevel level,
      memory_pressure::ReclaimTarget reclaim_target) override {
    SystemMemoryPressureEvaluator::OnMemoryPressure(level, reclaim_target);
  }

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;

  ~TestSystemMemoryPressureEvaluator() override = default;
};

TEST(ChromeOSSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  // We will use a mock listener to keep track of our kernel notifications which
  // cause event to be fired. We can just examine the sequence of pressure
  // events when we're done to validate that the pressure events were as
  // expected.
  std::vector<base::MemoryPressureListener::MemoryPressureLevel>
      pressure_events;
  auto listener = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&PressureCallback, &pressure_events));

  memory_pressure::MultiSourceMemoryPressureMonitor monitor;

  auto evaluator = std::make_unique<TestSystemMemoryPressureEvaluator>(
      /*for_testing=*/true, monitor.CreateVoter());

  // At this point we have no memory pressure.
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Moderate Pressure.
  evaluator->OnMemoryPressure(PressureLevel::MODERATE,
                              memory_pressure::ReclaimTarget(1000));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // Critical Pressure.
  evaluator->OnMemoryPressure(PressureLevel::CRITICAL,
                              memory_pressure::ReclaimTarget(1000));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator->current_vote());

  // Moderate Pressure.
  evaluator->OnMemoryPressure(PressureLevel::MODERATE,
                              memory_pressure::ReclaimTarget(1000));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // No pressure, note: this will not cause any event.
  evaluator->OnMemoryPressure(PressureLevel::NONE,
                              memory_pressure::ReclaimTarget(0));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Back into moderate.
  evaluator->OnMemoryPressure(PressureLevel::MODERATE,
                              memory_pressure::ReclaimTarget(1000));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // Now our events should be MODERATE, CRITICAL, MODERATE.
  ASSERT_EQ(4u, pressure_events.size());
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            pressure_events[0]);
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            pressure_events[1]);
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            pressure_events[2]);
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            pressure_events[3]);
}

}  // namespace memory
}  // namespace ash
