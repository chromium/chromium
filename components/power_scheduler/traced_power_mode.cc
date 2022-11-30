// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/traced_power_mode.h"

#include "base/trace_event/trace_event.h"
#include "components/power_scheduler/power_mode.h"

namespace power_scheduler {

TracedPowerMode::TracedPowerMode(const char* name, const void* trace_id)
    : name_(name), trace_id_(trace_id), mode_(PowerMode::kIdle) {
  DCHECK(name_);
  OnTraceLogEnabled();  // In case it's already enabled.
}

TracedPowerMode::~TracedPowerMode() {
  if (!name_)
    return;
  TRACE_EVENT_NESTABLE_ASYNC_END0("power", PowerModeToString(mode_), trace_id_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("power", name_, trace_id_);
}

TracedPowerMode::TracedPowerMode(TracedPowerMode&& other)
    : name_(other.name_), trace_id_(other.trace_id_), mode_(other.mode_) {
  other.name_ = nullptr;
}

void TracedPowerMode::OnTraceLogEnabled() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("power", name_, trace_id_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("power", PowerModeToString(mode_),
                                    trace_id_);
  incremental_state_cleared_ = false;
}

void TracedPowerMode::OnIncrementalStateCleared() {
  incremental_state_cleared_ = true;
}

void TracedPowerMode::SetMode(PowerMode mode) {
  if (mode_ == mode)
    return;
  TRACE_EVENT_NESTABLE_ASYNC_END0("power", PowerModeToString(mode_), trace_id_);
  if (incremental_state_cleared_) {
    // Restart the top-level event to ensure track names remain correct in ring
    // buffer traces.
    // TODO(eseckler): Use perfetto::Track with a custom name here instead once
    // supported by slow reports (b/205542316).
    TRACE_EVENT_NESTABLE_ASYNC_END0("power", name_, trace_id_);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("power", name_, trace_id_);
    incremental_state_cleared_ = false;
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("power", PowerModeToString(mode),
                                    trace_id_);
  mode_ = mode;
}

}  // namespace power_scheduler
