// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

#include <optional>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

TEST(MultiSourceMemoryPressureMonitorTest, NoEvaluatorUponConstruction) {
  MultiSourceMemoryPressureMonitor monitor;
  EXPECT_FALSE(monitor.system_evaluator_for_testing());
}

TEST(MultiSourceMemoryPressureMonitorTest, RunDispatchCallback) {
#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, the previous SingleThreadTaskEnvironment was sufficient.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
#else
  // On other platforms (like Mac), the full TaskEnvironment is needed for the
  // ThreadPool.
  base::test::TaskEnvironment task_environment;
#endif

  MultiSourceMemoryPressureMonitor monitor;
  bool callback_called = false;
  monitor.SetDispatchCallbackForTesting(base::BindLambdaForTesting(
      [&](base::MemoryPressureLevel) { callback_called = true; }));
  monitor.MaybeStartPlatformVoter();
  auto* const aggregator = monitor.aggregator_for_testing();

  aggregator->OnVoteForTesting(std::nullopt, base::MEMORY_PRESSURE_LEVEL_NONE);
  aggregator->NotifyListenersForTesting();
  EXPECT_TRUE(callback_called);

  // Clear vote so aggregator's destructor doesn't think there are loose voters.
  aggregator->OnVoteForTesting(base::MEMORY_PRESSURE_LEVEL_NONE, std::nullopt);
}

// Voter attribution is only wired up on macOS.
#if BUILDFLAG(IS_MAC)
TEST(MultiSourceMemoryPressureMonitorTest, VoterAttributionMetrics) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MultiSourceMemoryPressureMonitor monitor;
  base::HistogramTester histogram_tester;

  auto avail_voter =
      monitor.CreateVoter(MemoryPressureVoterType::kAvailableMemory);
  auto comp_voter = monitor.CreateVoter(MemoryPressureVoterType::kCompressor);

  // 1. AvailableMemory votes MODERATE.
  avail_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE,
                       /*notify_listeners=*/false);
  task_environment.FastForwardBy(base::Seconds(10));

  // 2. Transition from MODERATE to CRITICAL by Compressor voting CRITICAL.
  comp_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                      /*notify_listeners=*/false);

  // At this transition, 10 seconds of MODERATE are reported.
  // AvailableMemory was voting MODERATE, so Memory.PressureVoter.Moderate
  // should record 10 seconds for kAvailableMemory.
  histogram_tester.ExpectBucketCount(
      "Memory.PressureVoter.Moderate",
      static_cast<int>(MemoryPressureVoterType::kAvailableMemory), 10);
  histogram_tester.ExpectTotalCount("Memory.PressureVoter.Critical", 0);

  // 3. Stay in CRITICAL for 20 seconds.
  task_environment.FastForwardBy(base::Seconds(20));

  // 4. AvailableMemory also votes CRITICAL (overlapping voters).
  avail_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                       /*notify_listeners=*/false);

  // 20 seconds of CRITICAL reported for Compressor.
  histogram_tester.ExpectBucketCount(
      "Memory.PressureVoter.Critical",
      static_cast<int>(MemoryPressureVoterType::kCompressor), 20);

  // 5. Advance 15 seconds while both voters are CRITICAL.
  task_environment.FastForwardBy(base::Seconds(15));

  // Return to NONE.
  comp_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE,
                      /*notify_listeners=*/false);
  avail_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_NONE,
                       /*notify_listeners=*/false);

  // In the 15-second overlapping period, both voters should be attributed 15
  // seconds. Compressor: 20s + 15s = 35s total. AvailableMemory: 15s total.
  histogram_tester.ExpectBucketCount(
      "Memory.PressureVoter.Critical",
      static_cast<int>(MemoryPressureVoterType::kCompressor), 35);
  histogram_tester.ExpectBucketCount(
      "Memory.PressureVoter.Critical",
      static_cast<int>(MemoryPressureVoterType::kAvailableMemory), 15);

  // Clean up voters before monitor destruction.
  avail_voter.reset();
  comp_voter.reset();
}

TEST(MultiSourceMemoryPressureMonitorTest,
     VoterAttributionPeriodicFlushAndVoterRemoval) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MultiSourceMemoryPressureMonitor monitor;
  base::HistogramTester histogram_tester;

  auto avail_voter =
      monitor.CreateVoter(MemoryPressureVoterType::kAvailableMemory);
  auto comp_voter = monitor.CreateVoter(MemoryPressureVoterType::kCompressor);

  // 1. AvailableMemory votes MODERATE and keeps that vote. The 5-minute
  // periodic flush attributes the elapsed time without any vote change.
  avail_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE,
                       /*notify_listeners=*/false);
  task_environment.FastForwardBy(base::Minutes(5));

  histogram_tester.ExpectBucketCount(
      "Memory.PressureVoter.Moderate",
      static_cast<int>(MemoryPressureVoterType::kAvailableMemory), 300);

  // 2. Compressor votes CRITICAL and is then destroyed while still voting.
  // Removing the vote flushes the elapsed CRITICAL time to Compressor first.
  comp_voter->SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                      /*notify_listeners=*/false);
  task_environment.FastForwardBy(base::Seconds(20));
  comp_voter.reset();

  histogram_tester.ExpectBucketCount(
      "Memory.PressureVoter.Critical",
      static_cast<int>(MemoryPressureVoterType::kCompressor), 20);
  // Time at CRITICAL is not credited to voters at MODERATE.
  histogram_tester.ExpectBucketCount(
      "Memory.PressureVoter.Moderate",
      static_cast<int>(MemoryPressureVoterType::kAvailableMemory), 300);

  // Clean up voters before monitor destruction.
  avail_voter.reset();
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace memory_pressure
