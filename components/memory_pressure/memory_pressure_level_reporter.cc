// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_level_reporter.h"

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace memory_pressure {

MemoryPressureLevelReporter::MemoryPressureLevelReporter(
    base::MemoryPressureLevel initial_pressure_level)
    : current_pressure_level_(initial_pressure_level) {
  StartPeriodicTimer();
}

MemoryPressureLevelReporter::~MemoryPressureLevelReporter() {
  // Make sure that the data about the last interval gets reported.
  ReportHistogram(base::TimeTicks::Now());
}

void MemoryPressureLevelReporter::OnMemoryPressureLevelChanged(
    base::MemoryPressureLevel new_level) {
  const base::TimeTicks now = base::TimeTicks::Now();
  ReportHistogram(now);

  // Records the duration of the latest pressure session, there are 4
  // transitions of interest:
  //   - Moderate -> None
  //   - Moderate -> Critical
  //   - Critical -> Moderate
  //   - Critical -> None
  // |new_level| can be equal to |periodic_reporting_timer_| if this gets called
  // by the one shot reporting timer, do nothing in this case.
  if ((new_level != current_pressure_level_) &&
      (current_pressure_level_ != base::MEMORY_PRESSURE_LEVEL_NONE)) {
    constexpr char kHistogramPrefix[] = "Memory.PressureWindowDuration.";
    std::string histogram_name;

    DCHECK(!current_pressure_level_begin_.is_null());
    switch (current_pressure_level_) {
      // From:
      case base::MEMORY_PRESSURE_LEVEL_MODERATE: {
        // To:
        if (new_level == base::MEMORY_PRESSURE_LEVEL_NONE) {
          histogram_name = base::StrCat({kHistogramPrefix, "ModerateToNone"});
        } else {  // base::MEMORY_PRESSURE_LEVEL_CRITICAL
          histogram_name =
              base::StrCat({kHistogramPrefix, "ModerateToCritical"});
        }
        break;
      }
      // From:
      case base::MEMORY_PRESSURE_LEVEL_CRITICAL: {
        // To:
        if (new_level == base::MEMORY_PRESSURE_LEVEL_NONE) {
          histogram_name = base::StrCat({kHistogramPrefix, "CriticalToNone"});
        } else {  // base::MEMORY_PRESSURE_LEVEL_MODERATE
          histogram_name =
              base::StrCat({kHistogramPrefix, "CriticalToModerate"});
        }
        break;
      }
      case base::MEMORY_PRESSURE_LEVEL_NONE:
      default:
        NOTREACHED();
    }

    base::UmaHistogramCustomTimes(histogram_name,
                                  now - current_pressure_level_begin_,
                                  base::Seconds(1), base::Minutes(10), 50);
  }

  current_pressure_level_begin_ = now;
  current_pressure_level_ = new_level;

  StartPeriodicTimer();
}

void MemoryPressureLevelReporter::ReportHistogram(base::TimeTicks now) {
  auto duration = now - current_pressure_level_begin_;
  auto duration_s = duration.InSeconds();
  accumulator_buckets_[current_pressure_level_] +=
      duration - base::Seconds(duration_s);
  auto accumulated_seconds =
      accumulator_buckets_[current_pressure_level_].InSeconds();
  if (accumulated_seconds > 0) {
    duration_s += accumulated_seconds;
    accumulator_buckets_[current_pressure_level_] -=
        base::Seconds(accumulated_seconds);
  }

  if (duration_s) {
    // We can't use UmaHistogramEnumeration here as it doesn't support
    // |AddCount|.
    base::LinearHistogram::FactoryGet(
        "Memory.PressureLevel2", 1, base::MemoryPressureLevel::kMaxValue + 1,
        base::MemoryPressureLevel::kMaxValue + 2,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->AddCount(current_pressure_level_,
                   base::saturated_cast<int>(duration_s));
  }
}

void MemoryPressureLevelReporter::StartPeriodicTimer() {
  // Don't try to start the timer in tests that don't support it.
  if (!base::SequencedTaskRunner::HasCurrentDefault()) {
    return;
  }
  periodic_reporting_timer_.Start(
      FROM_HERE, base::Minutes(5),
      base::BindOnce(&MemoryPressureLevelReporter::OnMemoryPressureLevelChanged,
                     // Unretained is safe because |this| owns this timer.
                     base::Unretained(this), current_pressure_level_));
}

}  // namespace memory_pressure
