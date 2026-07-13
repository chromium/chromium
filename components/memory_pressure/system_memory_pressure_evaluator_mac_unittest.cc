// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_mac.h"

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {
namespace mac {

class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::
      MemoryPressureLevelForMacMemoryPressureLevel;

  TestSystemMemoryPressureEvaluator(std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)) {}

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;

  // A HistogramTester for verifying correct UMA stat generation.
  base::HistogramTester tester;

  // Sets the raw macOS memory pressure level read by the memory pressure
  // evaluator.
  int macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_NORMAL;

  // Exposes the UpdatePressureLevel() method for testing.
  void UpdatePressureLevel() {
    SystemMemoryPressureEvaluator::UpdatePressureLevel();
  }

  // Exposes the OnDiskSpaceCheckComplete() method for testing.
  void TriggerDiskSpaceCheckComplete(
      std::optional<base::SysInfo::DiskSpaceInfo> disk_space_info) {
    OnDiskSpaceCheckComplete(disk_space_info);
  }

 private:
  int GetMacMemoryPressureLevel() override {
    return macos_pressure_level_for_testing_;
  }
};

TEST(MacSystemMemoryPressureEvaluatorTest,
     MemoryPressureFromMacMemoryPressure) {
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(
                    DISPATCH_MEMORYPRESSURE_NORMAL));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(
                    DISPATCH_MEMORYPRESSURE_WARN));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(
                    DISPATCH_MEMORYPRESSURE_CRITICAL));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(0));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(3));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(5));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(-1));
}

TEST(MacSystemMemoryPressureEvaluatorTest, CurrentMemoryPressure) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  base::MemoryPressureLevel memory_pressure = evaluator.current_vote();
  EXPECT_TRUE(memory_pressure == base::MEMORY_PRESSURE_LEVEL_NONE ||
              memory_pressure == base::MEMORY_PRESSURE_LEVEL_MODERATE ||
              memory_pressure == base::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST(MacSystemMemoryPressureEvaluatorTest, MemoryPressureConversion) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_NORMAL;
  evaluator.UpdatePressureLevel();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());

  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_WARN;
  evaluator.UpdatePressureLevel();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());

  evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_CRITICAL;
  evaluator.UpdatePressureLevel();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
}

TEST(MacSystemMemoryPressureEvaluatorTest, OSTransitionsOnly) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MultiSourceMemoryPressureMonitor monitor;
  base::HistogramTester histogram_tester;

  TestSystemMemoryPressureEvaluator evaluator(monitor.CreateVoter());

  // 1. Simulate OS transition: NONE -> MODERATE.
  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_WARN;
  evaluator.UpdatePressureLevel();

  // No transition reported yet (just entered MODERATE).
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration.ModerateToCritical", 0);

  // Advance clock in MODERATE.
  task_environment.FastForwardBy(base::Seconds(15));

  // 2. Simulate OS transition: MODERATE -> CRITICAL (OS).
  evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_CRITICAL;
  evaluator.UpdatePressureLevel();

  // Should report ModerateToCritical (15s) from OS signal.
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToCritical", base::Seconds(15), 1);

  // 3. Now simulate disk pressure while OS is CRITICAL.
  // Disk becomes low (should vote CRITICAL, but OS is already CRITICAL).
  base::SysInfo::DiskSpaceInfo disk_space_info;
  disk_space_info.available = base::MiBU(100);
  evaluator.TriggerDiskSpaceCheckComplete(disk_space_info);

  // Advance clock in CRITICAL (both OS and Disk are CRITICAL).
  task_environment.FastForwardBy(base::Seconds(20));

  // 4. Simulate Disk pressure goes away, but OS remains CRITICAL.
  disk_space_info.available = base::MiBU(500);
  evaluator.TriggerDiskSpaceCheckComplete(disk_space_info);

  // No transition should be reported because OS level didn't change (remained
  // CRITICAL).
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration.CriticalToNone", 0);
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration.CriticalToModerate", 0);

  // Advance clock.
  task_environment.FastForwardBy(base::Seconds(10));

  // 5. Simulate OS transition: CRITICAL -> NONE.
  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_NORMAL;
  evaluator.UpdatePressureLevel();

  // Should report CriticalToNone (20s from step 3-4 + 10s from step 4-5 = 30s).
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.CriticalToNone", base::Seconds(30), 1);
}

}  // namespace mac
}  // namespace memory_pressure
