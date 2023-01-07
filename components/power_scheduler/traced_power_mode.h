// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_TRACED_POWER_MODE_H_
#define COMPONENTS_POWER_SCHEDULER_TRACED_POWER_MODE_H_

#include <atomic>

#include "base/memory/raw_ptr.h"
#include "components/power_scheduler/power_mode.h"

namespace power_scheduler {

// Keeps track of a PowerMode and traces its value transitions. Not thread-safe.
class TracedPowerMode {
 public:
  TracedPowerMode(const char* name, const void* trace_id);
  ~TracedPowerMode();

  TracedPowerMode(const TracedPowerMode&) = delete;
  TracedPowerMode(TracedPowerMode&&);

  void OnTraceLogEnabled();
  void OnIncrementalStateCleared();

  void SetMode(PowerMode);
  PowerMode mode() const { return mode_; }

 private:
  const char* name_;
  raw_ptr<const void> trace_id_;
  PowerMode mode_;
  bool incremental_state_cleared_ = false;
};

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_TRACED_POWER_MODE_H_
