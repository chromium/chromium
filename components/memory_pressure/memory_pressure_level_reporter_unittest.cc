// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_level_reporter.h"

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

TEST(MemoryPressureLevelReporterTest, PressureWindowDuration) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::HistogramTester histogram_tester;

  // Moderate -> None.
  task_environment.AdvanceClock(base::Seconds(12));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToNone", base::Seconds(12), 1);

  // Moderate -> Critical.
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment.AdvanceClock(base::Seconds(20));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToCritical", base::Seconds(20), 1);

  // Critical -> None
  task_environment.AdvanceClock(base::Seconds(25));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.CriticalToNone", base::Seconds(25), 1);

  // Critical -> Moderate
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment.AdvanceClock(base::Seconds(27));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.CriticalToModerate", base::Seconds(27), 1);
}

TEST(MemoryPressureLevelReporterTest, MemoryPressureHistogram) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::unique_ptr<MemoryPressureLevelReporter> reporter =
      std::make_unique<MemoryPressureLevelReporter>(
          base::MEMORY_PRESSURE_LEVEL_NONE);
  base::HistogramTester histogram_tester;

  constexpr base::TimeDelta kDelay = base::Seconds(12);
  const char* kHistogram = "Memory.PressureLevel2";

  // None -> Moderate.
  task_environment.AdvanceClock(kDelay);
  reporter->OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  // There one report for a |kdelay| MEMORY_PRESSURE_LEVEL_NONE session.
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_NONE),
      kDelay.InSeconds());

  task_environment.AdvanceClock(kDelay);
  reporter->OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);
  // There one report for a |kdelay| MEMORY_PRESSURE_LEVEL_MODERATE session.
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_MODERATE),
      kDelay.InSeconds());

  task_environment.AdvanceClock(kDelay);
  reporter->OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // There's now two reports for a |kdelay| MEMORY_PRESSURE_LEVEL_NONE session,
  // for a total of |2*kdelay|.
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_NONE),
      (2 * kDelay).InSeconds());

  task_environment.AdvanceClock(kDelay);
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_CRITICAL), 0);
  reporter.reset();
  // Releasing the reporter should report the data from the current pressure
  // session.
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_CRITICAL),
      kDelay.InSeconds());
}

TEST(MemoryPressureLevelReporterTest, MemoryPressureHistogramAccumulatedTime) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(base::MEMORY_PRESSURE_LEVEL_NONE);
  base::HistogramTester histogram_tester;

  const char* kHistogram = "Memory.PressureLevel2";
  constexpr base::TimeDelta kHalfASecond = base::Milliseconds(500);

  task_environment.AdvanceClock(kHalfASecond);
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  // The delay is inferior to one second, there should be no data reported.
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_NONE), 0);

  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);
  task_environment.AdvanceClock(kHalfASecond);
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  // The delay is inferior to one second, there should be no data reported.
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_NONE), 1);
}

TEST(MemoryPressureLevelReporterTest,
     MemoryPressureHistogramPeriodicReporting) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(base::MEMORY_PRESSURE_LEVEL_NONE);
  base::HistogramTester histogram_tester;

  const char* kHistogram = "Memory.PressureLevel2";

  // Advancing the clock by a few seconds shouldn't cause any periodic
  // reporting.
  task_environment.FastForwardBy(base::Seconds(10));
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_NONE), 0);

  // Advancing the clock by a few minutes should cause periodic reporting.
  task_environment.FastForwardBy(base::Minutes(5));
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_NONE),
      5 * 60 /* 5 minutes */);

  task_environment.FastForwardBy(base::Minutes(5));
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_NONE),
      2 * 5 * 60 /* 2 x 5 minutes */);
}

// Test that verifies the integer overflow fix using base::saturated_cast
TEST(MemoryPressureLevelReporterTest, IntegerOverflowFixWithSaturatedCast) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter{base::MEMORY_PRESSURE_LEVEL_MODERATE};
  base::HistogramTester histogram_tester;

  // Test case 1: Normal duration (should work as before)
  task_environment.AdvanceClock(base::Hours(1));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);

  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(base::MEMORY_PRESSURE_LEVEL_MODERATE), 3600);

  // Test case 2: Duration exceeding INT_MAX (overflow scenario)
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  constexpr int64_t overflow_duration =
      std::numeric_limits<int>::max() + 1000LL;
  task_environment.AdvanceClock(base::Seconds(overflow_duration));

  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);

  // With base::saturated_cast fix, this should be clamped to INT_MAX
  // instead of overflowing to a negative or wrapped value
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(base::MEMORY_PRESSURE_LEVEL_CRITICAL),
      std::numeric_limits<int>::max());  // Should be saturated to INT_MAX

  // Verify that no negative values are recorded due to overflow
  auto samples = histogram_tester.GetAllSamples("Memory.PressureLevel2");
  for (const auto& sample : samples) {
    EXPECT_GT(sample.count, 0)
        << "All counts should be positive after saturated cast fix";
  }
}

}  // namespace memory_pressure
