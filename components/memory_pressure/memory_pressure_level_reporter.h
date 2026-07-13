// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_

#include <array>

#include "base/memory/memory_pressure_listener.h"
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
      base::MemoryPressureLevel initial_pressure_level);
  ~MemoryPressureLevelReporter();

  // Should be called whenever the current memory pressure level changes.
  void OnMemoryPressureLevelChanged(base::MemoryPressureLevel new_level);

  // Called when disk pressure state changes. |new_os_pressure_level| indicates
  // the current OS-reported memory pressure level, so that time is only
  // attributed to the disk bucket when the OS is not also critical.
  void UpdateDiskPressureState(bool new_is_disk_pressure,
                               base::MemoryPressureLevel new_os_pressure_level);

 private:
  void ReportHistogram(base::TimeTicks now);
  void StartPeriodicTimer();

  base::MemoryPressureLevel current_pressure_level_;
  base::TimeTicks current_pressure_level_begin_ = base::TimeTicks::Now();

  // Tracks whether the current critical pressure is due to low disk space.
  // When true, |os_pressure_level_| indicates the OS-reported level so that
  // time is only attributed to the disk bucket when the OS is not also
  // critical.
  bool is_disk_pressure_ = false;
  base::MemoryPressureLevel os_pressure_level_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;

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
