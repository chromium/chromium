// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/input_state.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace omnibox {

InputState::InputState() = default;

size_t EstimateMemoryUsage(const ToolConfig& config) {
  return base::trace_event::EstimateMemoryUsage(config.menu_label()) +
         base::trace_event::EstimateMemoryUsage(config.chip_label()) +
         base::trace_event::EstimateMemoryUsage(config.hint_text());
}

size_t EstimateMemoryUsage(const ModelConfig& config) {
  return base::trace_event::EstimateMemoryUsage(config.menu_label()) +
         base::trace_event::EstimateMemoryUsage(config.hint_text());
}

size_t EstimateMemoryUsage(const SectionConfig& config) {
  return base::trace_event::EstimateMemoryUsage(config.header());
}

size_t EstimateMemoryUsage(const InputTypeConfig& config) {
  return base::trace_event::EstimateMemoryUsage(config.menu_label());
}

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
  res += base::trace_event::EstimateMemoryUsage(tool_configs);
  res += base::trace_event::EstimateMemoryUsage(model_configs);
  res += base::trace_event::EstimateMemoryUsage(input_type_configs);
  if (tools_section_config.has_value()) {
    res += omnibox::EstimateMemoryUsage(tools_section_config.value());
  }
  if (model_section_config.has_value()) {
    res += omnibox::EstimateMemoryUsage(model_section_config.value());
  }
  res += base::trace_event::EstimateMemoryUsage(max_instances);
  res += base::trace_event::EstimateMemoryUsage(hint_text);

  return res;
}

ModelMode InputState::GetDefaultModel() const {
  if (!allowed_models.empty()) {
    return allowed_models[0];
  }
  return ModelMode::MODEL_MODE_UNSPECIFIED;
}

}  // namespace omnibox
