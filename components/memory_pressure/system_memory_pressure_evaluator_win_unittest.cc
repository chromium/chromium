// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_win.h"

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/memory/available_memory_monitor.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {
namespace win {

namespace {

constexpr char kCommitLimitMBHistogramName[] = "Memory.CommitLimitMB";
constexpr char kCommitAvailableMBHistogramName[] = "Memory.CommitAvailableMB";
constexpr char kCommitPercentageUsedHistogramName[] =
    "Memory.CommitPercentageUsed";

}  // namespace

class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel;
  using SystemMemoryPressureEvaluator::CheckMemoryPressure;
  using SystemMemoryPressureEvaluator::RecordCommitHistograms;

  explicit TestSystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)) {
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(base::ByteSize system_memory,
                                    base::ByteSize moderate_threshold,
                                    base::ByteSize critical_threshold)
      : SystemMemoryPressureEvaluator(moderate_threshold,
                                      critical_threshold,
                                      nullptr) {
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;

  base::AvailableMemoryMonitor::MemorySample CreateSample(
      base::ByteSize phys_left,
      base::ByteSize commit_limit = base::GiB(4),
      base::ByteSize commit_available = base::GiB(2)) {
    base::AvailableMemoryMonitor::MemorySample sample;
    sample.timestamp = base::TimeTicks::Now();
    sample.available_physical_bytes = phys_left;
    sample.available_commit_bytes = commit_available;
    sample.total_commit_bytes = commit_limit;
    return sample;
  }

  base::AvailableMemoryMonitor::MemorySample SampleNone() {
    return CreateSample(moderate_threshold() + base::MiB(1));
  }

  base::AvailableMemoryMonitor::MemorySample SampleModerate() {
    return CreateSample((moderate_threshold() - base::MiB(1)).AsByteSize());
  }

  base::AvailableMemoryMonitor::MemorySample SampleCritical() {
    return CreateSample((critical_threshold() - base::MiB(1)).AsByteSize());
  }
};

class WinSystemMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestSystemMemoryPressureEvaluator* evaluator) {
    base::ByteSize moderate = evaluator->moderate_threshold();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(moderate + base::MiB(1))));

    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(moderate)));

    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel(evaluator->CreateSample(
                  (moderate - base::MiB(1)).AsByteSize())));

    base::ByteSize critical = evaluator->critical_threshold();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(critical + base::MiB(1))));

    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel(
                  evaluator->CreateSample(critical)));

    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel(evaluator->CreateSample(
                  (critical - base::MiB(1)).AsByteSize())));
  }

  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

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

TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static constexpr base::ByteSize kSystem = base::MiB(512);
  static constexpr base::ByteSize kModerate = base::MiB(256);
  static constexpr base::ByteSize kCritical = base::MiB(128);

  TestSystemMemoryPressureEvaluator evaluator(kSystem, kModerate, kCritical);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

TEST_F(WinSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  MultiSourceMemoryPressureMonitor monitor;

  TestSystemMemoryPressureEvaluator evaluator(monitor.CreateVoter());

  testing::StrictMock<base::RegisteredMockMemoryPressureListener> listener;

  // Checking the memory pressure at 0% load should not produce any events.
  evaluator.CheckMemoryPressure(evaluator.SampleNone());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());

  // Setting the memory level to 80% should produce a moderate pressure level.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.CheckMemoryPressure(evaluator.SampleModerate());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Check that the event gets reposted after the cooldown period (10s).
  for (int i = 1; i <= 5; ++i) {
    if (i == 5) {
      EXPECT_CALL(listener,
                  OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    task_environment_.FastForwardBy(base::Seconds(2));
    evaluator.CheckMemoryPressure(evaluator.SampleModerate());
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&listener);
  }

  // Setting the memory usage to 99% should produce critical levels.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.CheckMemoryPressure(evaluator.SampleCritical());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Check that the critical event gets reposted after the cooldown period (5s).
  for (int i = 1; i <= 3; ++i) {
    if (i == 3) {
      EXPECT_CALL(listener,
                  OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
    }
    task_environment_.FastForwardBy(base::Seconds(2));
    evaluator.CheckMemoryPressure(evaluator.SampleCritical());
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&listener);
  }

  // When lowering the pressure again there should be a notification and the
  // pressure should go back to moderate.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.CheckMemoryPressure(evaluator.SampleModerate());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Check that the event gets reposted after the cooldown period (10s).
  for (int i = 1; i <= 5; ++i) {
    if (i == 5) {
      EXPECT_CALL(listener,
                  OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    task_environment_.FastForwardBy(base::Seconds(2));
    evaluator.CheckMemoryPressure(evaluator.SampleModerate());
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&listener);
  }

  // Going down to no pressure should produce a notification.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE));
  evaluator.CheckMemoryPressure(evaluator.SampleNone());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Again no pressure should not produce an additional notification.
  task_environment_.FastForwardBy(
      MemoryPressureVoteAggregator::kModeratePressureCooldown);
  evaluator.CheckMemoryPressure(evaluator.SampleNone());
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);
}

TEST_F(WinSystemMemoryPressureEvaluatorTest, RecordCommitHistogramsBasic) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/base::GiB(4),
                                       /*commit_available=*/base::GiB(2));

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 4096, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 2048, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 50,
                                      1);
}

TEST_F(WinSystemMemoryPressureEvaluatorTest,
       RecordCommitHistogramsDivisionByZero) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/base::ByteSize(0),
                                       /*commit_available=*/base::ByteSize(0));

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

TEST_F(WinSystemMemoryPressureEvaluatorTest, RecordCommitHistogramsOverflow) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  constexpr base::ByteSize kLargerThanMaxInt =
      base::MiB(static_cast<uint64_t>(std::numeric_limits<int>::max()) + 1U);
  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/kLargerThanMaxInt,
                                       /*commit_available=*/kLargerThanMaxInt);

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 10000000, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 10000000,
                                      1);
}

TEST_F(WinSystemMemoryPressureEvaluatorTest, PotentialUnderflow) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  auto sample = evaluator.CreateSample(/*phys_left=*/base::GiB(4),
                                       /*commit_limit=*/base::MiB(50),
                                       /*commit_available=*/base::MiB(100));

  evaluator.RecordCommitHistograms(sample);

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 50, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 100, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

}  // namespace win
}  // namespace memory_pressure
