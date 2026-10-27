// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/disk_space_memory_pressure_evaluator_mac.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/byte_size.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure::mac {

class TestDiskSpaceMemoryPressureEvaluator
    : public DiskSpaceMemoryPressureEvaluator {
 public:
  explicit TestDiskSpaceMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : DiskSpaceMemoryPressureEvaluator(std::move(voter)) {}

  TestDiskSpaceMemoryPressureEvaluator(
      const TestDiskSpaceMemoryPressureEvaluator&) = delete;
  TestDiskSpaceMemoryPressureEvaluator& operator=(
      const TestDiskSpaceMemoryPressureEvaluator&) = delete;

  void TriggerDiskSpaceCheckComplete(
      std::optional<base::SysInfo::DiskSpaceInfo> disk_space_info) {
    OnDiskSpaceCheckComplete(disk_space_info);
  }
};

TEST(MacDiskSpaceMemoryPressureEvaluatorTest, DiskSpacePressureConversion) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
  MultiSourceMemoryPressureMonitor monitor;

  TestDiskSpaceMemoryPressureEvaluator evaluator(
      monitor.CreateVoter(/*is_disk_voter=*/true));
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());

  // Simulate ample disk space (500 MiB).
  base::SysInfo::DiskSpaceInfo disk_space_info;
  disk_space_info.available = base::MiB(500);
  evaluator.TriggerDiskSpaceCheckComplete(disk_space_info);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            monitor.aggregator_for_testing()->EvaluateVotesForTesting());

  // Simulate critical disk space (< 250 MiB).
  disk_space_info.available = base::MiB(100);
  evaluator.TriggerDiskSpaceCheckComplete(disk_space_info);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
            monitor.aggregator_for_testing()->EvaluateVotesForTesting());

  // Simulate recovery of disk space.
  disk_space_info.available = base::MiB(500);
  evaluator.TriggerDiskSpaceCheckComplete(disk_space_info);
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
            monitor.aggregator_for_testing()->EvaluateVotesForTesting());
}

}  // namespace memory_pressure::mac
