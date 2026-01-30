// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_INPUT_STATE_H_
#define COMPONENTS_OMNIBOX_COMMON_INPUT_STATE_H_

#include <cstddef>
#include <vector>

#include "third_party/omnibox_proto/aim_input_types.pb.h"
#include "third_party/omnibox_proto/aim_models.pb.h"
#include "third_party/omnibox_proto/aim_tools.pb.h"

namespace omnibox {

// Represents a valid searchbox inputs state.
struct InputState {
  InputState();
  InputState(const InputState&);
  InputState& operator=(const InputState&);
  ~InputState();

  size_t EstimateMemoryUsage() const;

  // The set of allowed tools, models, and input types.
  std::vector<ToolMode> allowed_tools;
  std::vector<ModelMode> allowed_models;
  std::vector<InputType> allowed_input_types;
  // The currently active tool and model.
  ToolMode active_tool = ToolMode::TOOL_MODE_UNSPECIFIED;
  ModelMode active_model = ModelMode::MODEL_MODE_UNSPECIFIED;
  // The set of currently disabled tools, models, and input types.
  std::vector<ToolMode> disabled_tools;
  std::vector<ModelMode> disabled_models;
  std::vector<InputType> disabled_input_types;
};

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_INPUT_STATE_H_
