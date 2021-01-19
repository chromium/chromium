// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_MODE_TRACER_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_MODE_TRACER_H_

#include <atomic>

#include "base/trace_event/trace_event.h"
#include "components/power_scheduler/power_mode.h"

namespace power_scheduler {

// Keeps track of a PowerMode and traces its value transitions. Not thread-safe.
class TracedPowerMode {
 public:
  explicit TracedPowerMode(const char* name, const void* trace_id);
  ~TracedPowerMode();

  void OnTraceLogEnabled() const;

  void SetMode(PowerMode);
  PowerMode mode() const { return mode_; }

 private:
  const char* name_;
  const void* trace_id_;
  PowerMode mode_;
};

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_MODE_TRACER_H_
