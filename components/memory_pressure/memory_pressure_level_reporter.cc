// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_level_reporter.h"

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_level.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace memory_pressure {

MemoryPressureLevelReporter::MemoryPressureLevelReporter(
    base::MemoryPressureLevel initial_pressure_level,
    std::optional<std::string> histogram_name,
    std::optional<std::string> transition_prefix)
    : histogram_name_(std::move(histogram_name)),
      transition_prefix_(std::move(transition_prefix)),
      current_pressure_level_(initial_pressure_level) {
  StartPeriodicTimer();
}

MemoryPressureLevelReporter::~MemoryPressureLevelReporter() {
  // Make sure that the data about the last interval gets reported.
  ReportHistogram(base::TimeTicks::Now());
}

void MemoryPressureLevelReporter::OnMemoryPressureLevelChanged(
    base::MemoryPressureLevel new_level,
    bool new_disk_only_critical) {
  DCHECK(!new_disk_only_critical ||
         new_level == base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  const base::TimeTicks now = base::TimeTicks::Now();
  // Attributes the elapsed interval to the previous level and disk state.
  ReportHistogram(now);

  // Records the duration of the latest pressure session, there are 4
  // transitions of interest:
  //   - Moderate -> None
  //   - Moderate -> Critical
  //   - Critical -> Moderate
  //   - Critical -> None
  if (transition_prefix_.has_value() &&
      (new_level != current_pressure_level_) &&
      (current_pressure_level_ != base::MEMORY_PRESSURE_LEVEL_NONE)) {
    std::string histogram_name;

    DCHECK(!current_pressure_level_begin_.is_null());
    switch (current_pressure_level_) {
      // From:
      case base::MEMORY_PRESSURE_LEVEL_MODERATE: {
        // To:
        if (new_level == base::MEMORY_PRESSURE_LEVEL_NONE) {
          histogram_name =
              base::StrCat({*transition_prefix_, "ModerateToNone"});
        } else {  // base::MEMORY_PRESSURE_LEVEL_CRITICAL
          histogram_name =
              base::StrCat({*transition_prefix_, "ModerateToCritical"});
        }
        break;
      }
      // From:
      case base::MEMORY_PRESSURE_LEVEL_CRITICAL: {
        // To:
        if (new_level == base::MEMORY_PRESSURE_LEVEL_NONE) {
          histogram_name =
              base::StrCat({*transition_prefix_, "CriticalToNone"});
        } else {  // base::MEMORY_PRESSURE_LEVEL_MODERATE
          histogram_name =
              base::StrCat({*transition_prefix_, "CriticalToModerate"});
        }
        break;
      }
      case base::MEMORY_PRESSURE_LEVEL_NONE:
      default:
        NOTREACHED();
    }

    base::UmaHistogramCustomTimes(histogram_name,
                                  now - current_pressure_level_begin_,
                                  base::Seconds(1), base::Hours(12), 100);
  }

  if (new_level != current_pressure_level_) {
    current_pressure_level_begin_ = now;
    current_pressure_level_ = new_level;
  }
  is_disk_only_critical_ = new_disk_only_critical;

  StartPeriodicTimer();
}

void MemoryPressureLevelReporter::SetDiskOnlyCritical(
    bool new_disk_only_critical) {
  if (is_disk_only_critical_ == new_disk_only_critical) {
    return;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  ReportHistogram(now);
  is_disk_only_critical_ = new_disk_only_critical;
}

void MemoryPressureLevelReporter::ReportHistogram(base::TimeTicks now) {
  auto duration = now - last_report_time_;
  last_report_time_ = now;
  auto duration_s = duration.InSeconds();

  MemoryPressureHistogramBuckets bucket;
  switch (current_pressure_level_) {
    case base::MEMORY_PRESSURE_LEVEL_NONE:
      bucket = MemoryPressureHistogramBuckets::kNone;
      break;
    case base::MEMORY_PRESSURE_LEVEL_MODERATE:
      bucket = MemoryPressureHistogramBuckets::kModerate;
      break;
    case base::MEMORY_PRESSURE_LEVEL_CRITICAL:
      bucket = is_disk_only_critical_
                   ? MemoryPressureHistogramBuckets::kDisk
                   : MemoryPressureHistogramBuckets::kCritical;
      break;
  }
  size_t index = static_cast<size_t>(bucket);

  accumulator_buckets_[index] += duration - base::Seconds(duration_s);
  auto accumulated_seconds = accumulator_buckets_[index].InSeconds();
  if (accumulated_seconds > 0) {
    duration_s += accumulated_seconds;
    accumulator_buckets_[index] -= base::Seconds(accumulated_seconds);
  }

  if (duration_s && histogram_name_.has_value()) {
    // We can't use UmaHistogramEnumeration here as it doesn't support
    // |AddCount|.
    const int max_value =
        static_cast<int>(MemoryPressureHistogramBuckets::kMaxValue);
    base::LinearHistogram::FactoryGet(
        *histogram_name_, 1, max_value + 1, max_value + 2,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->AddCount(static_cast<int>(bucket),
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
      base::BindOnce(&MemoryPressureLevelReporter::OnPeriodicReportingTimer,
                     // Unretained is safe because |this| owns this timer.
                     base::Unretained(this)));
}

void MemoryPressureLevelReporter::OnPeriodicReportingTimer() {
  ReportHistogram(base::TimeTicks::Now());
  StartPeriodicTimer();
}

}  // namespace memory_pressure
