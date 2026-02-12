// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_INPUT_STATE_H_
#define COMPONENTS_OMNIBOX_COMMON_INPUT_STATE_H_

#include <cstddef>
#include <map>
#include <optional>
#include <vector>

#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/input_type_config.pb.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/section_config.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

namespace omnibox {

// Represents a valid searchbox inputs state.
// LINT.IfChange(InputState)
struct InputState {
  InputState();
  InputState(const InputState&);
  InputState& operator=(const InputState&);
  ~InputState();

  size_t EstimateMemoryUsage() const;

  // Helper method to derive the default model from the allowed list.
  ModelMode GetDefaultModel() const;

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
  // Configs containing the header, chip label, and hint text.
  std::vector<ToolConfig> tool_configs;
  std::vector<ModelConfig> model_configs;
  std::vector<InputTypeConfig> input_type_configs;
  std::optional<SectionConfig> tools_section_config;
  std::optional<SectionConfig> model_section_config;
  // The max number of inputs of a given type.
  std::map<InputType, int> max_instances;
  int max_total_inputs = 0;
  std::string hint_text;

  // Returns whether both `TOOL_MODE_IMAGE_GEN` and `INPUT_TYPE_LENS_IMAGE` are
  // active. Needed for suggest requests with `TOOL_MODE_IMAGE_GEN_UPLOAD`.
  bool image_gen_upload_active = false;
};
// LINT.ThenChange(//components/omnibox/composebox/composebox_query.mojom:InputState)

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_INPUT_STATE_H_
