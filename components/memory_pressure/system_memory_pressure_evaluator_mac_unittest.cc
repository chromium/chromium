// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_mac.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/disk_space_memory_pressure_evaluator_mac.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {
namespace mac {

class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::
      MemoryPressureLevelForMacMemoryPressureLevel;
  using SystemMemoryPressureEvaluator::OnMemoryPressureChanged;
  using SystemMemoryPressureEvaluator::UpdatePressureLevel;

  explicit TestSystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
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

 private:
  int GetMacMemoryPressureLevel() override {
    return macos_pressure_level_for_testing_;
  }
};

class TestMacAuxiliaryDiskEvaluator : public DiskSpaceMemoryPressureEvaluator {
 public:
  explicit TestMacAuxiliaryDiskEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : DiskSpaceMemoryPressureEvaluator(std::move(voter)) {}

  void TriggerDiskSpaceCheckComplete(
      std::optional<base::SysInfo::DiskSpaceInfo> disk_space_info) {
    OnDiskSpaceCheckComplete(disk_space_info);
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
  TestMacAuxiliaryDiskEvaluator disk_evaluator(
      monitor.CreateVoter(/*is_disk_voter=*/true));

  // 1. Simulate OS transition: NONE -> MODERATE.
  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_WARN;
  evaluator.UpdatePressureLevel();

  // No transition reported yet (just entered MODERATE).
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration2.ModerateToCritical", 0);

  // Advance clock in MODERATE.
  task_environment.FastForwardBy(base::Seconds(15));

  // 2. Simulate OS transition: MODERATE -> CRITICAL (OS).
  evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_CRITICAL;
  evaluator.UpdatePressureLevel();

  // Should report ModerateToCritical (15s) from OS signal.
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration2.ModerateToCritical", base::Seconds(15),
      1);

  // 3. Now simulate disk pressure while OS is CRITICAL.
  // Disk becomes low (should vote CRITICAL, but OS is already CRITICAL).
  base::SysInfo::DiskSpaceInfo disk_space_info;
  disk_space_info.available = base::MiB(100);
  disk_evaluator.TriggerDiskSpaceCheckComplete(disk_space_info);

  // Advance clock in CRITICAL (both OS and Disk are CRITICAL).
  task_environment.FastForwardBy(base::Seconds(20));

  // 4. Simulate Disk pressure goes away, but OS remains CRITICAL.
  disk_space_info.available = base::MiB(500);
  disk_evaluator.TriggerDiskSpaceCheckComplete(disk_space_info);

  // No transition should be reported because OS level didn't change (remained
  // CRITICAL).
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration2.CriticalToNone", 0);
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration2.CriticalToModerate", 0);

  // Advance clock.
  task_environment.FastForwardBy(base::Seconds(10));

  // 5. Simulate OS transition: CRITICAL -> NONE.
  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_NORMAL;
  evaluator.UpdatePressureLevel();

  // Should report CriticalToNone (20s from step 3-4 + 10s from step 4-5 = 30s).
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration2.CriticalToNone", base::Seconds(30), 1);

  // The same reporter also accumulates time spent at each OS level.
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2.OS",
      static_cast<int>(MemoryPressureHistogramBuckets::kModerate), 15);
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2.OS",
      static_cast<int>(MemoryPressureHistogramBuckets::kCritical), 30);
}

TEST(MacSystemMemoryPressureEvaluatorTest, ShadowModeEvaluator) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MultiSourceMemoryPressureMonitor monitor;
  base::HistogramTester histogram_tester;

  bool dispatch_called = false;
  monitor.SetDispatchCallbackForTesting(base::BindLambdaForTesting(
      [&](base::MemoryPressureLevel) { dispatch_called = true; }));

  TestSystemMemoryPressureEvaluator shadow_evaluator(/*voter=*/nullptr);

  // Initial OS pressure is NORMAL -> NONE.
  EXPECT_FALSE(dispatch_called);

  // OS moves to WARN -> MODERATE.
  shadow_evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_WARN;
  shadow_evaluator.OnMemoryPressureChanged();

  // A shadow evaluator must not influence the aggregated pressure level.
  EXPECT_FALSE(dispatch_called);

  task_environment.FastForwardBy(base::Seconds(12));

  // OS moves to CRITICAL.
  shadow_evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_CRITICAL;
  shadow_evaluator.OnMemoryPressureChanged();

  EXPECT_FALSE(dispatch_called);

  // But it still reports the OS signal.
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2.OS",
      static_cast<int>(MemoryPressureHistogramBuckets::kModerate), 12);

  task_environment.FastForwardBy(base::Seconds(18));

  // OS moves back to NORMAL.
  shadow_evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_NORMAL;
  shadow_evaluator.OnMemoryPressureChanged();

  EXPECT_FALSE(dispatch_called);

  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2.OS",
      static_cast<int>(MemoryPressureHistogramBuckets::kCritical), 18);
}

TEST(MacSystemMemoryPressureEvaluatorTest,
     ShadowModeDoesNotAffectDiskOrCriticalBuckets) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MultiSourceMemoryPressureMonitor monitor;
  base::HistogramTester histogram_tester;

  TestSystemMemoryPressureEvaluator shadow_evaluator(/*voter=*/nullptr);
  TestMacAuxiliaryDiskEvaluator disk_evaluator(
      monitor.CreateVoter(/*is_disk_voter=*/true));
  auto memory_voter = monitor.CreateVoter();

  // Case 1: Both disk and the active memory voter are CRITICAL while the
  // shadow OS evaluator is NORMAL. Should be reported as kCritical (not kDisk).
  base::SysInfo::DiskSpaceInfo low_disk;
  low_disk.available = base::MiB(100);
  disk_evaluator.TriggerDiskSpaceCheckComplete(low_disk);
  memory_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                        /*notify_listeners=*/false);
  task_environment.FastForwardBy(base::Seconds(10));

  // Case 2: Active memory voter drops to NONE while disk stays CRITICAL, and
  // the shadow OS evaluator transitions to CRITICAL. Since the shadow OS
  // evaluator does not vote, the next 15s should be reported as kDisk (not
  // kCritical).
  memory_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE,
                        /*notify_listeners=*/false);
  shadow_evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_CRITICAL;
  shadow_evaluator.OnMemoryPressureChanged();
  task_environment.FastForwardBy(base::Seconds(15));

  // Recover disk space to flush the second interval.
  base::SysInfo::DiskSpaceInfo ample_disk;
  ample_disk.available = base::MiB(500);
  disk_evaluator.TriggerDiskSpaceCheckComplete(ample_disk);

  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(MemoryPressureHistogramBuckets::kCritical), 10);
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(MemoryPressureHistogramBuckets::kDisk), 15);

  memory_voter.reset();
}

}  // namespace mac
}  // namespace memory_pressure
