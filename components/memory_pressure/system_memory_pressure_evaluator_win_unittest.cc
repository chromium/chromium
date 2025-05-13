// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_win.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace memory_pressure {
namespace win {

namespace {

struct PressureSettings {
  int phys_left_mb;
  base::MemoryPressureListener::MemoryPressureLevel level;
};

const char kCommitLimitMBHistogramName[] = "Memory.CommitLimitMB";
const char kCommitAvailableMBHistogramName[] = "Memory.CommitAvailableMB";
const char kCommitPercentageUsedHistogramName[] = "Memory.CommitPercentageUsed";

}  // namespace

// This is outside of the anonymous namespace so that it can be seen as a friend
// to the evaluator class.
class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel;
  using SystemMemoryPressureEvaluator::CheckMemoryPressure;
  using SystemMemoryPressureEvaluator::RecordCommitHistograms;

  static const DWORDLONG kMBBytes = 1024 * 1024;

  explicit TestSystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)), mem_status_() {
    // Generate a plausible amount of memory.
    mem_status_.ullTotalPhys = 8000 * kMBBytes;

    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(int system_memory_mb,
                                    int moderate_threshold_mb,
                                    int critical_threshold_mb)
      : SystemMemoryPressureEvaluator(moderate_threshold_mb,
                                      critical_threshold_mb,
                                      nullptr),
        mem_status_() {
    // Set the amount of system memory.
    mem_status_.ullTotalPhys =
        static_cast<DWORDLONG>(system_memory_mb * kMBBytes);

    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;

  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel level));

  // Sets up the memory status to reflect the provided absolute memory left.
  void SetMemoryFree(int phys_left_mb) {
    // ullTotalPhys is set in the constructor and not modified.

    // Set the amount of available memory.
    mem_status_.ullAvailPhys = static_cast<DWORDLONG>(phys_left_mb) * kMBBytes;
    DCHECK_LT(mem_status_.ullAvailPhys, mem_status_.ullTotalPhys);

    // These fields are unused.
    mem_status_.dwMemoryLoad = 0;
    mem_status_.ullTotalVirtual = 0;
    mem_status_.ullAvailVirtual = 0;
  }

  // Sets up the memory status to reflect commit limit and available.
  void SetCommitData(uint64_t commit_limit_mb, uint64_t commit_available_mb) {
    mem_status_.ullTotalPageFile = commit_limit_mb * kMBBytes;
    mem_status_.ullAvailPageFile = commit_available_mb * kMBBytes;
  }

  void SetNone() { SetMemoryFree(moderate_threshold_mb() + 1); }

  void SetModerate() { SetMemoryFree(moderate_threshold_mb() - 1); }

  void SetCritical() { SetMemoryFree(critical_threshold_mb() - 1); }

  MEMORYSTATUSEX GetSystemMemoryStatusForTesting() { return mem_status_; }

 private:
  bool GetSystemMemoryStatus(MEMORYSTATUSEX* mem_status) override {
    // Simply copy the memory status set by the test fixture.
    *mem_status = mem_status_;
    return true;
  }

  MEMORYSTATUSEX mem_status_;
};

class WinSystemMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestSystemMemoryPressureEvaluator* evaluator) {
    int mod = evaluator->moderate_threshold_mb();
    evaluator->SetMemoryFree(mod + 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(mod);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(mod - 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    int crit = evaluator->critical_threshold_mb();
    evaluator->SetMemoryFree(crit + 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(crit);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(crit - 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

// Tests the fundamental direct calculation of memory pressure with default
// thresholds.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelDefault) {
  static const int kModerateMb =
      SystemMemoryPressureEvaluator::kPhysicalMemoryDefaultModerateThresholdMb;
  static const int kCriticalMb =
      SystemMemoryPressureEvaluator::kPhysicalMemoryDefaultCriticalThresholdMb;

  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  EXPECT_EQ(kModerateMb, evaluator.moderate_threshold_mb());
  EXPECT_EQ(kCriticalMb, evaluator.critical_threshold_mb());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// Tests the fundamental direct calculation of memory pressure with manually
// specified threshold levels.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static const int kSystemMb = 512;
  static const int kModerateMb = 256;
  static const int kCriticalMb = 128;

  TestSystemMemoryPressureEvaluator evaluator(kSystemMb, kModerateMb,
                                              kCriticalMb);

  EXPECT_EQ(kModerateMb, evaluator.moderate_threshold_mb());
  EXPECT_EQ(kCriticalMb, evaluator.critical_threshold_mb());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// This test tests the various transition states from memory pressure, looking
// for the correct behavior on event reposting as well as state updates.
TEST_F(WinSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  MultiSourceMemoryPressureMonitor monitor;

  testing::StrictMock<TestSystemMemoryPressureEvaluator> evaluator(
      monitor.CreateVoter());

  base::MemoryPressureListener listener(
      FROM_HERE,
      base::BindRepeating(&TestSystemMemoryPressureEvaluator::OnMemoryPressure,
                          base::Unretained(&evaluator)));

  // Checking the memory pressure at 0% load should not produce any
  // events.
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.current_vote());

  // Setting the memory level to 80% should produce a moderate pressure level.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // Check that the event gets reposted after a while.
  const int kModeratePressureCooldownCycles =
      evaluator.kModeratePressureCooldown / evaluator.kDefaultPeriod;

  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(
          evaluator,
          OnMemoryPressure(
              base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&evaluator);
  }

  // Setting the memory usage to 99% should produce critical levels.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.SetCritical();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // Calling it again should immediately produce a second call.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // When lowering the pressure again there should be a notification and the
  // pressure should go back to moderate.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // Check that the event gets reposted after a while.
  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(
          evaluator,
          OnMemoryPressure(
              base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&evaluator);
  }

  // Going down to no pressure should not produce an notification.
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);
}

// RecordCommitHistograms emits the correct histograms when
// GetSystemMemoryStatus succeeds.
TEST_F(WinSystemMemoryPressureEvaluatorTest, RecordCommitHistogramsBasic) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.SetCommitData(/*commit_limit_mb=*/4096,
                          /*commit_available_mb=*/2048);

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

  evaluator.SetCommitData(/*commit_limit_mb=*/0, /*commit_available_mb=*/0);

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

  constexpr uint64_t kLargerThanMaxInt =
      static_cast<uint64_t>(std::numeric_limits<int>::max()) + 1U;
  evaluator.SetCommitData(/*commit_limit_mb=*/kLargerThanMaxInt,
                          /*commit_available_mb=*/kLargerThanMaxInt);

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

  evaluator.SetCommitData(/*commit_limit_mb=*/50,
                          /*commit_available_mb=*/100);

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 50, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 100, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

}  // namespace win
}  // namespace memory_pressure
