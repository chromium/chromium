// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_win.h"

#include <windows.h>

#include <ntstatus.h>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {
namespace win {

namespace {

struct PressureSettings {
  base::ByteSize phys_left;
  base::MemoryPressureLevel level;
};

constexpr char kCommitLimitMBHistogramName[] = "Memory.CommitLimitMB";
constexpr char kCommitAvailableMBHistogramName[] = "Memory.CommitAvailableMB";
constexpr char kCommitPercentageUsedHistogramName[] =
    "Memory.CommitPercentageUsed";

}  // namespace

// This is outside of the anonymous namespace so that it can be seen as a friend
// to the evaluator class.
class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel;
  using SystemMemoryPressureEvaluator::CheckMemoryPressure;
  using SystemMemoryPressureEvaluator::RecordCommitHistograms;

  explicit TestSystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)), mem_status_() {
    // Generate a plausible amount of memory.
    mem_status_.ullTotalPhys = base::MiBU(8000).InBytes();

    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(base::ByteSize system_memory,
                                    base::ByteSize moderate_threshold,
                                    base::ByteSize critical_threshold)
      : SystemMemoryPressureEvaluator(moderate_threshold,
                                      critical_threshold,
                                      nullptr),
        mem_status_() {
    // Set the amount of system memory.
    mem_status_.ullTotalPhys = system_memory.InBytes();

    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;

  // Sets up the memory status to reflect the provided absolute memory left.
  void SetMemoryFree(base::ByteSize phys_left) {
    // ullTotalPhys is set in the constructor and not modified.

    // Set the amount of available memory.
    mem_status_.ullAvailPhys = phys_left.InBytes();
    DCHECK_LT(mem_status_.ullAvailPhys, mem_status_.ullTotalPhys);

    // These fields are unused.
    mem_status_.dwMemoryLoad = 0;
    mem_status_.ullTotalVirtual = 0;
    mem_status_.ullAvailVirtual = 0;
  }

  // Sets up the memory status to reflect commit limit and available.
  void SetCommitData(base::ByteSize commit_limit,
                     base::ByteSize commit_available) {
    mem_status_.ullTotalPageFile = commit_limit.InBytes();
    mem_status_.ullAvailPageFile = commit_available.InBytes();
  }

  void SetNone() { SetMemoryFree(moderate_threshold() + base::MiBU(1)); }

  void SetModerate() {
    SetMemoryFree((moderate_threshold() - base::MiBU(1)).AsByteSize());
  }

  void SetCritical() {
    SetMemoryFree((critical_threshold() - base::MiBU(1)).AsByteSize());
  }

  MEMORYSTATUSEX GetSystemMemoryStatusForTesting() { return mem_status_; }

 private:
  bool GetSystemMemoryStatus(MEMORYSTATUSEX& mem_status) override {
    // Simply copy the memory status set by the test fixture.
    mem_status = mem_status_;
    return true;
  }

  MEMORYSTATUSEX mem_status_{};
};

class WinSystemMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestSystemMemoryPressureEvaluator* evaluator) {
    base::ByteSize moderate = evaluator->moderate_threshold();
    evaluator->SetMemoryFree(moderate + base::MiBU(1));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(moderate);
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree((moderate - base::MiBU(1)).AsByteSize());
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    base::ByteSize critical = evaluator->critical_threshold();
    evaluator->SetMemoryFree(critical + base::MiBU(1));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(critical);
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree((critical - base::MiBU(1)).AsByteSize());
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());
  }

  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

// Tests the fundamental direct calculation of memory pressure with default
// thresholds.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelDefault) {
  static constexpr base::ByteSize kModerate =
      SystemMemoryPressureEvaluator::kPhysicalMemoryDefaultModerateThreshold;
  static constexpr base::ByteSize kCritical =
      SystemMemoryPressureEvaluator::kPhysicalMemoryDefaultCriticalThreshold;

  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// Tests the fundamental direct calculation of memory pressure with manually
// specified threshold levels.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static constexpr base::ByteSize kSystem = base::MiBU(512);
  static constexpr base::ByteSize kModerate = base::MiBU(256);
  static constexpr base::ByteSize kCritical = base::MiBU(128);

  TestSystemMemoryPressureEvaluator evaluator(kSystem, kModerate, kCritical);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// This test tests the various transition states from memory pressure, looking
// for the correct behavior on event reposting as well as state updates.
TEST_F(WinSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  MultiSourceMemoryPressureMonitor monitor;

  TestSystemMemoryPressureEvaluator evaluator(monitor.CreateVoter());

  testing::StrictMock<base::RegisteredMockMemoryPressureListener> listener;

  // Checking the memory pressure at 0% load should not produce any
  // events.
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());

  // Setting the memory level to 80% should produce a moderate pressure level.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Check that the event gets reposted after a while.
  const int kModeratePressureCooldownCycles =
      evaluator.kModeratePressureCooldown / evaluator.kDefaultPeriod;

  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(listener,
                  OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&listener);
  }

  // Setting the memory usage to 99% should produce critical levels.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.SetCritical();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Calling it again should immediately produce a second call.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // When lowering the pressure again there should be a notification and the
  // pressure should go back to moderate.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Check that the event gets reposted after a while.
  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(listener,
                  OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&listener);
  }

  // Going down to no pressure should produce a notification.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE));
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Again no pressure should not produce an additional notification.
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);
}

// RecordCommitHistograms emits the correct histograms when
// GetSystemMemoryStatus succeeds.
TEST_F(WinSystemMemoryPressureEvaluatorTest, RecordCommitHistogramsBasic) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.SetCommitData(/*commit_limit=*/base::GiBU(4),
                          /*commit_available=*/base::GiBU(2));

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 4096, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 2048, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 50,
                                      1);
}

// Verifies behavior when commit limit is zero (division by zero).
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       RecordCommitHistogramsDivisionByZero) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.SetCommitData(/*commit_limit=*/base::ByteSize(0),
                          /*commit_available=*/base::ByteSize(0));

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

// RecordCommitHistograms should be able to handle commit values greater than
// 32-bit integers to calculate and correctly output all histograms.
TEST_F(WinSystemMemoryPressureEvaluatorTest, RecordCommitHistogramsOverflow) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  constexpr base::ByteSize kLargerThanMaxInt =
      base::MiBU(static_cast<uint64_t>(std::numeric_limits<int>::max()) + 1U);
  evaluator.SetCommitData(/*commit_limit=*/kLargerThanMaxInt,
                          /*commit_available=*/kLargerThanMaxInt);

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 10000000, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 10000000,
                                      1);
}

// Verifies that RecordCommitHistograms correctly handles the calculation of
// Memory.CommitPercentageUsed, specifically addressing the potential for
// underflow in that calculation.
TEST_F(WinSystemMemoryPressureEvaluatorTest, PotentialUnderflow) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.SetCommitData(/*commit_limit=*/base::MiBU(50),
                          /*commit_available=*/base::MiBU(100));

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 50, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 100, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

}  // namespace win
}  // namespace memory_pressure
