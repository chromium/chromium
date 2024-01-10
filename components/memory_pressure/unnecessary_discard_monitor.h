// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_UNNECESSARY_DISCARD_MONITOR_H_
#define COMPONENTS_MEMORY_PRESSURE_UNNECESSARY_DISCARD_MONITOR_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/memory_pressure/reclaim_target.h"

namespace memory_pressure {

// The UnnecessaryDiscardMonitor can be used to track and report metrics about
// tab discards that were unnecessary due to the age of the reclaim target that
// caused the discard. This is necessary because discarding a tab is slow, and
// tab discards from a given reclaim target can overlap with the calculation of
// subsequent reclaim targets. Here is an example scenario:
// - Reclaim target 1 is calculated with size 100.
// - Chrome receives reclaim target 1 and begins processing it.
// - Chrome determines that Tab A (size 200) must be discarded to fulfil the
// reclaim target of 100.
// - Chrome begins discarding Tab A.
// - Reclaim target 2 is calculated with size 110. At this point, Tab A has not
// yet been fully discarded and its memory has not been freed.
// - Chrome finishes discarding Tab A.
// - Chrome begins processing reclaim target 2.
// - Chrome determines that Tab B (size 200) must be discarded to fulfil the
// reclaim target of 110.
// - Chrome finishes discarding Tab B.
//
// In this example, discarding Tab B was unnecessary. Discarding Tab A alone
// would fulfil both reclaim targets since they were both calculated before Tab
// A's memory was freed.
class UnnecessaryDiscardMonitor {
 public:
  UnnecessaryDiscardMonitor();
  ~UnnecessaryDiscardMonitor();

  // Called when a reclaim target is received and processing starts. Optionally
  // can supply |process_start_time| to specify that processing of this reclaim
  // event started in the past.
  void OnReclaimTargetBegin(ReclaimTarget reclaim_target);

  // Called when processing a reclaim target finishes.
  // Calculates any unnecessary discards from the current reclaim target and
  // reports corresponding metrics.
  void OnReclaimTargetEnd();

  // Called when a tab is discarded. This discard is automatically associated
  // with the most recent reclaim target. |discard_complete_time| should be the
  // time that the tab discard completed.
  void OnDiscard(int64_t memory_freed_kb,
                 base::TimeTicks discard_complete_time);

 private:
  // Directly reports a metric with |count| unnecessary discards.
  void ReportUnnecessaryDiscards(size_t count);

  SEQUENCE_CHECKER(sequence_checker_);

  // The current reclaim event.
  std::optional<ReclaimTarget> current_reclaim_event_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Represents a single kill event.
  struct KillEvent {
    // The estimated size of the kill.
    int64_t kill_size_kb = 0;

    // The time at which the kill finished.
    base::TimeTicks kill_time;
  };

  // All kill events that happened before the current reclaim event was
  // processed.
  std::vector<KillEvent> previous_kill_events_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // All kills that are a result of the current reclaim event.
  std::vector<KillEvent> current_reclaim_event_kills_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_UNNECESSARY_DISCARD_MONITOR_H_
