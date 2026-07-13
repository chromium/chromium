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

TEST(MemoryPressureLevelReporterTest, MemoryPressureHistogramDiskSpace) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::unique_ptr<MemoryPressureLevelReporter> reporter =
      std::make_unique<MemoryPressureLevelReporter>(
          base::MEMORY_PRESSURE_LEVEL_NONE);
  base::HistogramTester histogram_tester;

  constexpr base::TimeDelta kDelay = base::Seconds(12);
  const char* kHistogram = "Memory.PressureLevel2";

  // NONE -> CRITICAL (simulated via disk pressure).
  reporter->UpdateDiskPressureState(true, base::MEMORY_PRESSURE_LEVEL_NONE);
  reporter->OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  task_environment.AdvanceClock(kDelay);
  reporter->OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);

  // Should have reported 12s of bucket 3 (DISK_CRITICAL)
  histogram_tester.ExpectBucketCount(kHistogram, 3, kDelay.InSeconds());
  // Should NOT have reported anything to bucket 2 (OS_CRITICAL)
  histogram_tester.ExpectBucketCount(
      kHistogram, static_cast<int>(base::MEMORY_PRESSURE_LEVEL_CRITICAL), 0);
}

TEST(MemoryPressureLevelReporterTest, PressureWindowDurationDiskSpace) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::HistogramTester histogram_tester;

  // Moderate -> Critical (simulated).
  reporter.UpdateDiskPressureState(true, base::MEMORY_PRESSURE_LEVEL_NONE);
  task_environment.AdvanceClock(base::Seconds(12));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // This transition should NOT be reported because is_disk_pressure_ is true.
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration.ModerateToCritical", 0);

  // Critical (simulated) -> None.
  task_environment.AdvanceClock(base::Seconds(20));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);

  // This transition should NOT be reported because is_disk_pressure_ was true
  // during it.
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration.CriticalToNone", 0);

  // Now set disk pressure inactive.
  reporter.UpdateDiskPressureState(false, base::MEMORY_PRESSURE_LEVEL_NONE);

  // Now do a normal transition: Moderate -> Critical (OS).
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment.AdvanceClock(base::Seconds(15));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // This transition SHOULD be reported!
  histogram_tester.ExpectTimeBucketCount(
      "Memory.PressureWindowDuration.ModerateToCritical", base::Seconds(15), 1);
}

TEST(MemoryPressureLevelReporterTest, DiskSpacePressureBucket) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MemoryPressureLevelReporter reporter(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::HistogramTester histogram_tester;

  // Disk pressure turns on while OS is at NONE.
  reporter.UpdateDiskPressureState(true, base::MEMORY_PRESSURE_LEVEL_NONE);
  task_environment.AdvanceClock(base::Seconds(10));
  reporter.UpdateDiskPressureState(false, base::MEMORY_PRESSURE_LEVEL_NONE);

  // Time should be attributed to the disk bucket (3), not critical (2).
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(MemoryPressureHistogramBuckets::kDisk), 10);
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(MemoryPressureHistogramBuckets::kCritical), 0);
}

TEST(MemoryPressureLevelReporterTest,
     DiskPressureDoesNotMaskOSCriticalPressure) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MemoryPressureLevelReporter reporter(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::HistogramTester histogram_tester;

  // Disk pressure turns on while OS is at NONE — should go to disk bucket.
  reporter.UpdateDiskPressureState(true, base::MEMORY_PRESSURE_LEVEL_NONE);
  task_environment.AdvanceClock(base::Seconds(10));

  // OS transitions to CRITICAL while disk pressure is still active.
  // The reporter should flush the 10s of disk-only time to bucket 3.
  reporter.UpdateDiskPressureState(true, base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment.AdvanceClock(base::Seconds(20));

  // Disk pressure turns off, OS is still CRITICAL.
  reporter.UpdateDiskPressureState(false, base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment.AdvanceClock(base::Seconds(5));

  // Flush the remaining 5s by transitioning to NONE.
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_NONE);

  // 10s should be in disk bucket (3), 25s should be in critical bucket (2).
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(MemoryPressureHistogramBuckets::kDisk), 10);
  histogram_tester.ExpectBucketCount(
      "Memory.PressureLevel2",
      static_cast<int>(MemoryPressureHistogramBuckets::kCritical), 25);
}

TEST(MemoryPressureLevelReporterTest, Customization) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  // Instantiate with both histogram and transition prefix disabled.
  MemoryPressureLevelReporter reporter(base::MEMORY_PRESSURE_LEVEL_MODERATE,
                                       std::nullopt, std::nullopt);
  base::HistogramTester histogram_tester;
  // Transition to CRITICAL.
  task_environment.AdvanceClock(base::Seconds(10));
  reporter.OnMemoryPressureLevelChanged(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // Verify no level histogram was reported.
  histogram_tester.ExpectTotalCount("Memory.PressureLevel2", 0);
  // Verify no transition was reported.
  histogram_tester.ExpectTotalCount(
      "Memory.PressureWindowDuration.ModerateToCritical", 0);
}

}  // namespace memory_pressure
