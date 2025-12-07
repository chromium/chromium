// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/pressure/system_memory_pressure_evaluator.h"

#include <unistd.h>

#include "base/byte_count.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {

namespace {

using PressureLevel = ResourcedClient::PressureLevel;

using testing::_;
using testing::Mock;

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

  memory_pressure::MultiSourceMemoryPressureMonitor monitor;

  auto evaluator = std::make_unique<TestSystemMemoryPressureEvaluator>(
      /*for_testing=*/true, monitor.CreateVoter());

  // At this point we have no memory pressure.
  ASSERT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator->current_vote());

  base::RegisteredMockMemoryPressureListener listener;

  // Moderate Pressure.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator->OnMemoryPressure(
      PressureLevel::MODERATE,
      memory_pressure::ReclaimTarget(base::ByteCount(1000)));
  ASSERT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator->current_vote());
  Mock::VerifyAndClearExpectations(&listener);

  // Critical Pressure.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator->OnMemoryPressure(
      PressureLevel::CRITICAL,
      memory_pressure::ReclaimTarget(base::ByteCount(1000)));
  ASSERT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator->current_vote());
  Mock::VerifyAndClearExpectations(&listener);

  // Moderate Pressure.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator->OnMemoryPressure(
      PressureLevel::MODERATE,
      memory_pressure::ReclaimTarget(base::ByteCount(1000)));
  ASSERT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator->current_vote());
  Mock::VerifyAndClearExpectations(&listener);

  // No pressure.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE));
  evaluator->OnMemoryPressure(
      PressureLevel::NONE, memory_pressure::ReclaimTarget(base::ByteCount(0)));
  ASSERT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator->current_vote());
  Mock::VerifyAndClearExpectations(&listener);

  // No pressure again, no notification.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE))
      .Times(0);
  evaluator->OnMemoryPressure(
      PressureLevel::NONE, memory_pressure::ReclaimTarget(base::ByteCount(0)));
  ASSERT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator->current_vote());
  Mock::VerifyAndClearExpectations(&listener);

  // Back into moderate.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator->OnMemoryPressure(
      PressureLevel::MODERATE,
      memory_pressure::ReclaimTarget(base::ByteCount(1000)));
  ASSERT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator->current_vote());
  Mock::VerifyAndClearExpectations(&listener);
}

}  // namespace memory
}  // namespace ash
