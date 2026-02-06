// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/contextual_search_mojom_traits.h"

#include "base/notreached.h"
#include "components/omnibox/composebox/composebox_query.mojom-shared.h"

namespace mojo {

namespace {

using UsedToolMode = composebox_query::mojom::ToolMode;
using UsedModelMode = composebox_query::mojom::ModelMode;
using UsedInputType = composebox_query::mojom::InputType;
using UsedToolConfigDataView = composebox_query::mojom::ToolConfigDataView;
using UsedModelConfigDataView = composebox_query::mojom::ModelConfigDataView;
using UsedSectionConfigDataView =
    composebox_query::mojom::SectionConfigDataView;
using UsedInputStateDataView = composebox_query::mojom::InputStateDataView;

}  // namespace

// static
UsedToolMode EnumTraits<UsedToolMode, omnibox::ToolMode>::ToMojom(
    omnibox::ToolMode input) {
  switch (input) {
    case omnibox::ToolMode::TOOL_MODE_UNSPECIFIED:
      return UsedToolMode::kUnspecified;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return UsedToolMode::kDeepSearch;
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return UsedToolMode::kCanvas;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
      return UsedToolMode::kImageGen;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD:
      return UsedToolMode::kImageGenUpload;
    // The proto compiler generates these sentinel values. We must handle them
    // to satisfy the compiler's exhaustiveness check (since we don't have a
    // default case), but they should never be encountered in practice.
    case omnibox::ToolMode::ToolMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case omnibox::ToolMode::ToolMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

// static
bool EnumTraits<UsedToolMode, omnibox::ToolMode>::FromMojom(
    UsedToolMode input,
    omnibox::ToolMode* output) {
  switch (input) {
    case UsedToolMode::kUnspecified:
      *output = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
      return true;
    case UsedToolMode::kDeepSearch:
      *output = omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH;
      return true;
    case UsedToolMode::kCanvas:
      *output = omnibox::ToolMode::TOOL_MODE_CANVAS;
      return true;
    case UsedToolMode::kImageGen:
      *output = omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
      return true;
    case UsedToolMode::kDeepBrowse:
      // No corresponding Proto value yet.
      *output = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
      return true;
    case UsedToolMode::kImageGenUpload:
      *output = omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD;
      return true;
    case UsedToolMode::kImageGenSelfie:
      // No corresponding Proto value yet.
      *output = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
      return true;
  }
  NOTREACHED();
}

// static
UsedModelMode EnumTraits<UsedModelMode, omnibox::ModelMode>::ToMojom(
    omnibox::ModelMode input) {
  switch (input) {
    case omnibox::ModelMode::MODEL_MODE_UNSPECIFIED:
      return UsedModelMode::kUnspecified;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
      return UsedModelMode::kGeminiRegular;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
      return UsedModelMode::kGeminiPro;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return UsedModelMode::kGeminiProAutoroute;
    // The proto compiler generates these sentinel values. We must handle them
    // to satisfy the compiler's exhaustiveness check (since we don't have a
    // default case), but they should never be encountered in practice.
    case omnibox::ModelMode::ModelMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case omnibox::ModelMode::ModelMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

// static
bool EnumTraits<UsedModelMode, omnibox::ModelMode>::FromMojom(
    UsedModelMode input,
    omnibox::ModelMode* output) {
  switch (input) {
    case UsedModelMode::kUnspecified:
      *output = omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
      return true;
    case UsedModelMode::kGeminiRegular:
      *output = omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR;
      return true;
    case UsedModelMode::kGeminiPro:
      *output = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO;
      return true;
    case UsedModelMode::kGeminiProAutoroute:
      *output = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE;
      return true;
  }
  NOTREACHED();
}

// static
UsedInputType EnumTraits<UsedInputType, omnibox::InputType>::ToMojom(
    omnibox::InputType input) {
  switch (input) {
    case omnibox::InputType::INPUT_TYPE_UNSPECIFIED:
      return UsedInputType::kUnspecified;
    case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
      return UsedInputType::kLensImage;
    case omnibox::InputType::INPUT_TYPE_LENS_FILE:
      return UsedInputType::kLensFile;
    case omnibox::InputType::INPUT_TYPE_BROWSER_TAB:
      return UsedInputType::kBrowserTab;
    // The proto compiler generates these sentinel values. We must handle them
    // to satisfy the compiler's exhaustiveness check (since we don't have a
    // default case), but they should never be encountered in practice.
    case omnibox::InputType::InputType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case omnibox::InputType::InputType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

// static
bool EnumTraits<UsedInputType, omnibox::InputType>::FromMojom(
    UsedInputType input,
    omnibox::InputType* output) {
  switch (input) {
    case UsedInputType::kUnspecified:
      *output = omnibox::InputType::INPUT_TYPE_UNSPECIFIED;
      return true;
    case UsedInputType::kLensImage:
      *output = omnibox::InputType::INPUT_TYPE_LENS_IMAGE;
      return true;
    case UsedInputType::kLensFile:
      *output = omnibox::InputType::INPUT_TYPE_LENS_FILE;
      return true;
    case UsedInputType::kBrowserTab:
      *output = omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
      return true;
  }
  NOTREACHED();
}

// static
omnibox::ToolMode
StructTraits<UsedToolConfigDataView, omnibox::ToolConfig>::tool(
    const omnibox::ToolConfig& config) {
  return config.tool();
}

// static
bool StructTraits<UsedToolConfigDataView, omnibox::ToolConfig>::
    disable_active_model_selection(const omnibox::ToolConfig& config) {
  return config.disable_active_model_selection();
}

// static
const std::string&
StructTraits<UsedToolConfigDataView, omnibox::ToolConfig>::menu_label(
    const omnibox::ToolConfig& config) {
  return config.menu_label();
}

// static
const std::string&
StructTraits<UsedToolConfigDataView, omnibox::ToolConfig>::chip_label(
    const omnibox::ToolConfig& config) {
  return config.chip_label();
}

// static
const std::string&
StructTraits<UsedToolConfigDataView, omnibox::ToolConfig>::hint_text(
    const omnibox::ToolConfig& config) {
  return config.hint_text();
}

// static
bool StructTraits<UsedToolConfigDataView, omnibox::ToolConfig>::Read(
    UsedToolConfigDataView data,
    omnibox::ToolConfig* output) {
  omnibox::ToolMode tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  if (!data.ReadTool(&tool)) {
    return false;
  }

  output->set_tool(tool);

  output->set_disable_active_model_selection(
      data.disable_active_model_selection());

  std::string menu_label;
  if (!data.ReadMenuLabel(&menu_label)) {
    return false;
  }
  output->set_menu_label(menu_label);

  std::string chip_label;
  if (!data.ReadChipLabel(&chip_label)) {
    return false;
  }
  output->set_chip_label(chip_label);

  std::string hint_text;
  if (!data.ReadHintText(&hint_text)) {
    return false;
  }
  output->set_hint_text(hint_text);

  return true;
}

// static
omnibox::ModelMode
StructTraits<UsedModelConfigDataView, omnibox::ModelConfig>::model(
    const omnibox::ModelConfig& config) {
  return config.model();
}

// static
const std::string&
StructTraits<UsedModelConfigDataView, omnibox::ModelConfig>::menu_label(
    const omnibox::ModelConfig& config) {
  return config.menu_label();
}

// static
const std::string&
StructTraits<UsedModelConfigDataView, omnibox::ModelConfig>::hint_text(
    const omnibox::ModelConfig& config) {
  return config.hint_text();
}

// static
bool StructTraits<UsedModelConfigDataView, omnibox::ModelConfig>::Read(
    UsedModelConfigDataView data,
    omnibox::ModelConfig* output) {
  omnibox::ModelMode model = omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  if (!data.ReadModel(&model)) {
    return false;
  }

  output->set_model(model);

  std::string menu_label;
  if (!data.ReadMenuLabel(&menu_label)) {
    return false;
  }
  output->set_menu_label(menu_label);

  std::string hint_text;
  if (!data.ReadHintText(&hint_text)) {
    return false;
  }
  output->set_hint_text(hint_text);

  return true;
}

// static
const std::string&
StructTraits<UsedSectionConfigDataView, omnibox::SectionConfig>::header(
    const omnibox::SectionConfig& config) {
  return config.header();
}

// static
bool StructTraits<UsedSectionConfigDataView, omnibox::SectionConfig>::Read(
    UsedSectionConfigDataView data,
    omnibox::SectionConfig* output) {
  std::string header;
  if (!data.ReadHeader(&header)) {
    return false;
  }
  output->set_header(header);
  return true;
}

// static
const std::vector<omnibox::ModelMode>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::allowed_models(
    const omnibox::InputState& input) {
  return input.allowed_models;
}

// static
const std::vector<omnibox::ToolMode>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::allowed_tools(
    const omnibox::InputState& input) {
  return input.allowed_tools;
}

// static
const std::vector<omnibox::InputType>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::allowed_input_types(
    const omnibox::InputState& input) {
  return input.allowed_input_types;
}

// static
omnibox::ModelMode
StructTraits<UsedInputStateDataView, omnibox::InputState>::active_model(
    const omnibox::InputState& input) {
  return input.active_model;
}

// static
omnibox::ToolMode
StructTraits<UsedInputStateDataView, omnibox::InputState>::active_tool(
    const omnibox::InputState& input) {
  return input.active_tool;
}

// static
const std::vector<omnibox::ModelMode>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::disabled_models(
    const omnibox::InputState& input) {
  return input.disabled_models;
}

// static
const std::vector<omnibox::ToolMode>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::disabled_tools(
    const omnibox::InputState& input) {
  return input.disabled_tools;
}

// static
const std::vector<omnibox::InputType>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::disabled_input_types(
    const omnibox::InputState& input) {
  return input.disabled_input_types;
}

// static
const std::vector<omnibox::ToolConfig>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::tool_configs(
    const omnibox::InputState& input) {
  return input.tool_configs;
}

// static
const std::vector<omnibox::ModelConfig>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::model_configs(
    const omnibox::InputState& input) {
  return input.model_configs;
}

// static
const std::optional<omnibox::SectionConfig>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::tools_section_config(
    const omnibox::InputState& input) {
  return input.tools_section_config;
}

// static
const std::optional<omnibox::SectionConfig>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::model_section_config(
    const omnibox::InputState& input) {
  return input.model_section_config;
}

// static
const std::string&
StructTraits<UsedInputStateDataView, omnibox::InputState>::hint_text(
    const omnibox::InputState& input) {
  return input.hint_text;
}

// static
bool StructTraits<UsedInputStateDataView, omnibox::InputState>::Read(
    UsedInputStateDataView data,
    omnibox::InputState* output) {
  return data.ReadAllowedModels(&output->allowed_models) &&
         data.ReadAllowedTools(&output->allowed_tools) &&
         data.ReadAllowedInputTypes(&output->allowed_input_types) &&
         data.ReadActiveModel(&output->active_model) &&
         data.ReadActiveTool(&output->active_tool) &&
         data.ReadDisabledModels(&output->disabled_models) &&
         data.ReadDisabledTools(&output->disabled_tools) &&
         data.ReadDisabledInputTypes(&output->disabled_input_types) &&
         data.ReadToolConfigs(&output->tool_configs) &&
         data.ReadModelConfigs(&output->model_configs) &&
         data.ReadToolsSectionConfig(&output->tools_section_config) &&
         data.ReadModelSectionConfig(&output->model_section_config) &&
         data.ReadHintText(&output->hint_text);
}

}  // namespace mojo
