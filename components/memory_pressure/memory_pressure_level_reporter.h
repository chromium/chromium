// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_

#include <array>
#include <optional>
#include <string>

#include "base/memory/memory_pressure_level.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace memory_pressure {

// Enums for the buckets of the "Memory.PressureLevel2" histogram.
// This is a subset of base::MemoryPressureLevel, plus additional simulated
// states.
enum class MemoryPressureHistogramBuckets {
  kNone = base::MEMORY_PRESSURE_LEVEL_NONE,
  kModerate = base::MEMORY_PRESSURE_LEVEL_MODERATE,
  kCritical = base::MEMORY_PRESSURE_LEVEL_CRITICAL,
  kDisk = base::MEMORY_PRESSURE_LEVEL_CRITICAL + 1,
  kMaxValue = kDisk,
};

// Report metrics related to memory pressure.
class MemoryPressureLevelReporter {
 public:
  explicit MemoryPressureLevelReporter(
      base::MemoryPressureLevel initial_pressure_level,
      std::optional<std::string> histogram_name = "Memory.PressureLevel2",
      std::optional<std::string> transition_prefix =
          "Memory.PressureWindowDuration2.");
  ~MemoryPressureLevelReporter();

  // Should be called whenever the current memory pressure level changes.
  // |new_disk_only_critical| is true if |new_level| is CRITICAL solely because
  // of low disk space. Both are updated together so the elapsed interval is
  // attributed to the previous state exactly once.
  void OnMemoryPressureLevelChanged(base::MemoryPressureLevel new_level,
                                    bool new_disk_only_critical = false);

  // Updates whether critical pressure is caused solely by low disk space, in
  // which case critical duration is attributed to the disk bucket. Used when
  // the level stays CRITICAL.
  void SetDiskOnlyCritical(bool new_disk_only_critical);

 private:
  void ReportHistogram(base::TimeTicks now);
  void StartPeriodicTimer();
  void OnPeriodicReportingTimer();

  const std::optional<std::string> histogram_name_;
  const std::optional<std::string> transition_prefix_;
  base::MemoryPressureLevel current_pressure_level_;

  // The timestamp when the current memory pressure level started. Used to
  // calculate the duration of memory pressure episodes for the
  // `Memory.PressureWindowDuration2.*` histograms upon a level transition.
  base::TimeTicks current_pressure_level_begin_ = base::TimeTicks::Now();

  // The timestamp of the last time ReportHistogram() accumulated time into
  // the `Memory.PressureLevel2` histogram. Decoupled from
  // `current_pressure_level_begin_` so periodic flushes do not move the
  // episode start marker.
  base::TimeTicks last_report_time_ = base::TimeTicks::Now();

  // Tracks whether the current critical pressure is caused solely by low disk
  // space.
  bool is_disk_only_critical_ = false;

  // The reporting of the pressure level histogram is done in seconds, the
  // duration in a given pressure state will be floored. This means that some
  // time will be truncated each time we send a report. This array is used to
  // accumulate the truncated time and add it to the reported value when it
  // exceeds one second.
  std::array<base::TimeDelta,
             static_cast<size_t>(MemoryPressureHistogramBuckets::kMaxValue) + 1>
      accumulator_buckets_;

  // Timer used to ensure a periodic reporting of the pressure level metric.
  // Without this there's a risk that a browser crash will cause some data to
  // be lost.
  base::OneShotTimer periodic_reporting_timer_;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_
