// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/input_state.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace omnibox {

InputState::InputState() = default;
InputState::InputState(const InputState&) = default;
InputState& InputState::operator=(const InputState&) = default;
InputState::~InputState() = default;

size_t InputState::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(allowed_tools);
  res += base::trace_event::EstimateMemoryUsage(allowed_models);
  res += base::trace_event::EstimateMemoryUsage(allowed_input_types);
  res += base::trace_event::EstimateMemoryUsage(disabled_tools);
  res += base::trace_event::EstimateMemoryUsage(disabled_models);
  res += base::trace_event::EstimateMemoryUsage(disabled_input_types);

  return res;
}

}  // namespace omnibox
