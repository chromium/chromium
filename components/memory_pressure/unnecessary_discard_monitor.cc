// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/unnecessary_discard_monitor.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace memory_pressure {
namespace {

// Don't expect any unnecessary discard values above 10.
constexpr size_t kUnnecessaryDiscardsExclusiveMax = 11;

}  // namespace

UnnecessaryDiscardMonitor::UnnecessaryDiscardMonitor() = default;
UnnecessaryDiscardMonitor::~UnnecessaryDiscardMonitor() = default;

void UnnecessaryDiscardMonitor::OnReclaimTargetBegin(
    ReclaimTarget reclaim_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Simply ignore if there is no timestamp for the reclaim target.
  if (!reclaim_target.origin_time) {
    return;
  }

  base::UmaHistogramMediumTimes(
      "Discarding.ReclaimTargetAge",
      base::TimeTicks::Now() - *reclaim_target.origin_time);

  // If the new reclaim event is younger than the most recent kill event, it
  // will have no unnecessary kills and the previous kill events list can be
  // cleared.
  if (!previous_kill_events_.empty() &&
      *reclaim_target.origin_time > previous_kill_events_.back().kill_time) {
    previous_kill_events_.clear();
  }

  // Store the reclaim target for use in identifiying unnecessary kills.
  current_reclaim_event_ = reclaim_target;
}

void UnnecessaryDiscardMonitor::OnReclaimTargetEnd() {
  // The end of a reclaim target means that any unnecessary discards from this
  // reclaim event can be calculated.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!current_reclaim_event_) {
    return;
  }

  int64_t adjusted_target_kb = current_reclaim_event_->target_kb;

  // Iterate through all previous kills to identify ones that were finished
  // after this reclaim target was created.
  for (const auto& kill_event : previous_kill_events_) {
    // If this kill happened after the current reclaim event was calculated,
    // the reclaim target should be adjusted.
    if (kill_event.kill_time >= current_reclaim_event_->origin_time) {
      adjusted_target_kb -= kill_event.kill_size_kb;
    }
  }

  // Now that the reclaim target has been adjusted by any kills that occurred
  // after it was calculated, we can check if any of its resultant kills were
  // unnecessary.
  size_t i = 0;
  for (; i < current_reclaim_event_kills_.size() && adjusted_target_kb > 0;
       i++) {
    adjusted_target_kb -= current_reclaim_event_kills_[i].kill_size_kb;
  }

  ReportUnnecessaryDiscards(current_reclaim_event_kills_.size() - i);

  // Now that the current reclaim event is finished, move its kills onto the
  // previous kills list.
  previous_kill_events_.reserve(previous_kill_events_.size() +
                                current_reclaim_event_kills_.size());
  std::move(std::begin(current_reclaim_event_kills_),
            std::end(current_reclaim_event_kills_),
            std::back_inserter(previous_kill_events_));
  current_reclaim_event_kills_.clear();
  current_reclaim_event_.reset();
}

void UnnecessaryDiscardMonitor::OnDiscard(
    int64_t memory_freed_kb,
    base::TimeTicks discard_complete_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_reclaim_event_) {
    // Cache this kill event along with the time it took place.
    current_reclaim_event_kills_.emplace_back(memory_freed_kb,
                                              discard_complete_time);
  }
}

void UnnecessaryDiscardMonitor::ReportUnnecessaryDiscards(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramExactLinear("Discarding.DiscardsDrivenByStaleSignal", count,
                                kUnnecessaryDiscardsExclusiveMax);
}

}  // namespace memory_pressure
