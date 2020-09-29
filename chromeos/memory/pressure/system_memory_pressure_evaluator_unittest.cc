// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/pressure/system_memory_pressure_evaluator.h"

#include <unistd.h>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace memory {

namespace {

// Processes OnMemoryPressure calls by just storing the sequence of events so we
// can validate that we received the expected pressure levels as the test runs.
void OnMemoryPressure(
    std::vector<base::MemoryPressureListener::MemoryPressureLevel>* history,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  history->push_back(level);
}

}  // namespace

class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  TestSystemMemoryPressureEvaluator(
      const std::string& mock_margin_file,
      bool disable_timer_for_testing,
      std::unique_ptr<util::MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(mock_margin_file,
                                      disable_timer_for_testing,
                                      std::move(voter)) {}

  static std::vector<int> GetMarginFileParts(const std::string& file) {
    return SystemMemoryPressureEvaluator::GetMarginFileParts(file);
  }

  void CheckMemoryPressureImpl(uint64_t mem_avail_mb) {
    SystemMemoryPressureEvaluator::CheckMemoryPressureImpl(mem_avail_mb);
  }

  ~TestSystemMemoryPressureEvaluator() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSystemMemoryPressureEvaluator);
};

TEST(ChromeOSSystemMemoryPressureEvaluatorTest, ParseMarginFileGood) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");

  ASSERT_TRUE(base::WriteFile(margin_file, "123"));
  const std::vector<int> parts1 =
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(
          margin_file.value());
  ASSERT_EQ(1u, parts1.size());
  ASSERT_EQ(123, parts1[0]);

  ASSERT_TRUE(base::WriteFile(margin_file, "123 456"));
  const std::vector<int> parts2 =
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(
          margin_file.value());
  ASSERT_EQ(2u, parts2.size());
  ASSERT_EQ(123, parts2[0]);
  ASSERT_EQ(456, parts2[1]);
}

TEST(ChromeOSSystemMemoryPressureEvaluatorTest, ParseMarginFileBad) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");

  // An empty margin file is bad.
  ASSERT_TRUE(base::WriteFile(margin_file, ""));
  ASSERT_TRUE(
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(margin_file.value())
          .empty());

  // The numbers will be in base10, so 4a6 would be invalid.
  ASSERT_TRUE(base::WriteFile(margin_file, "123 4a6"));
  ASSERT_TRUE(
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(margin_file.value())
          .empty());

  // The numbers must be integers.
  ASSERT_TRUE(base::WriteFile(margin_file, "123.2 412.3"));
  ASSERT_TRUE(
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(margin_file.value())
          .empty());
}

TEST(ChromeOSSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  // Create a temporary directory for our margin and available files.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");

  // Set the margin values to 500 (critical) and 1000 (moderate).
  const std::string kMarginContents = "500 1000";
  ASSERT_TRUE(base::WriteFile(margin_file, kMarginContents));

  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  // We will use a mock listener to keep track of our kernel notifications which
  // cause event to be fired. We can just examine the sequence of pressure
  // events when we're done to validate that the pressure events were as
  // expected.
  std::vector<base::MemoryPressureListener::MemoryPressureLevel>
      pressure_events;
  auto listener = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&OnMemoryPressure, &pressure_events));

  util::MultiSourceMemoryPressureMonitor monitor;
  monitor.ResetSystemEvaluatorForTesting();

  auto evaluator = std::make_unique<TestSystemMemoryPressureEvaluator>(
      margin_file.value(), /*disable_timer_for_testing=*/true,
      monitor.CreateVoter());

  // Validate that our margin levels are as expected after being parsed from our
  // synthetic margin file.
  ASSERT_EQ(500, evaluator->CriticalPressureThresholdMBForTesting());
  ASSERT_EQ(1000, evaluator->ModeratePressureThresholdMBForTesting());

  // At this point we have no memory pressure.
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Moderate Pressure.
  evaluator->CheckMemoryPressureImpl(900);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // Critical Pressure.
  evaluator->CheckMemoryPressureImpl(450);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator->current_vote());

  // Moderate Pressure.
  evaluator->CheckMemoryPressureImpl(550);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // No pressure, note: this will not cause any event.
  evaluator->CheckMemoryPressureImpl(1150);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Back into moderate.
  evaluator->CheckMemoryPressureImpl(950);
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
}  // namespace chromeos
