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
using UsedInputTypeConfigDataView =
    composebox_query::mojom::InputTypeConfigDataView;
using UsedSectionConfigDataView =
    composebox_query::mojom::SectionConfigDataView;
using UsedInputStateDataView = composebox_query::mojom::InputStateDataView;
using UsedFileUploadStatus = composebox_query::mojom::FileUploadStatus;
using UsedFileUploadErrorType = composebox_query::mojom::FileUploadErrorType;

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
    case omnibox::ToolMode::TOOL_MODE_DEEP_BROWSE:
      return UsedToolMode::kDeepBrowse;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_SELFIE:
      return UsedToolMode::kImageGenSelfie;
    case omnibox::ToolMode::TOOL_MODE_AIM:
      return UsedToolMode::kAim;
    case omnibox::ToolMode::TOOL_MODE_AIM_GEN_PROMPT:
      return UsedToolMode::kAimGenPrompt;
    case omnibox::ToolMode::TOOL_MODE_DISABLE_SUGGEST:
      return UsedToolMode::kDisableSuggest;
    case omnibox::ToolMode::TOOL_MODE_GEMINI_PRO:
      return UsedToolMode::kGeminiPro;
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
      *output = omnibox::ToolMode::TOOL_MODE_DEEP_BROWSE;
      return true;
    case UsedToolMode::kAim:
      *output = omnibox::ToolMode::TOOL_MODE_AIM;
      return true;
    case UsedToolMode::kAimGenPrompt:
      *output = omnibox::ToolMode::TOOL_MODE_AIM_GEN_PROMPT;
      return true;
    case UsedToolMode::kDisableSuggest:
      *output = omnibox::ToolMode::TOOL_MODE_DISABLE_SUGGEST;
      return true;
    case UsedToolMode::kGeminiPro:
      *output = omnibox::ToolMode::TOOL_MODE_GEMINI_PRO;
      return true;
    case UsedToolMode::kImageGenUpload:
      *output = omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD;
      return true;
    case UsedToolMode::kImageGenSelfie:
      *output = omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_SELFIE;
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
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
      return UsedModelMode::kGeminiProNoGenUi;
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
    case UsedModelMode::kGeminiProNoGenUi:
      *output = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;
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
UsedFileUploadStatus
EnumTraits<UsedFileUploadStatus, contextual_search::FileUploadStatus>::ToMojom(
    contextual_search::FileUploadStatus input) {
  switch (input) {
    case contextual_search::FileUploadStatus::kNotUploaded:
      return UsedFileUploadStatus::kNotUploaded;
    case contextual_search::FileUploadStatus::kProcessing:
      return UsedFileUploadStatus::kProcessing;
    case contextual_search::FileUploadStatus::kValidationFailed:
      return UsedFileUploadStatus::kValidationFailed;
    case contextual_search::FileUploadStatus::kUploadStarted:
      return UsedFileUploadStatus::kUploadStarted;
    case contextual_search::FileUploadStatus::kUploadSuccessful:
      return UsedFileUploadStatus::kUploadSuccessful;
    case contextual_search::FileUploadStatus::kUploadFailed:
      return UsedFileUploadStatus::kUploadFailed;
    case contextual_search::FileUploadStatus::kUploadExpired:
      return UsedFileUploadStatus::kUploadExpired;
    case contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady:
      return UsedFileUploadStatus::kProcessingSuggestSignalsReady;
    case contextual_search::FileUploadStatus::kUploadReplaced:
      return UsedFileUploadStatus::kUploadReplaced;
  }
  NOTREACHED();
}

// static
bool EnumTraits<UsedFileUploadStatus, contextual_search::FileUploadStatus>::
    FromMojom(UsedFileUploadStatus input,
              contextual_search::FileUploadStatus* output) {
  switch (input) {
    case UsedFileUploadStatus::kNotUploaded:
      *output = contextual_search::FileUploadStatus::kNotUploaded;
      return true;
    case UsedFileUploadStatus::kProcessing:
      *output = contextual_search::FileUploadStatus::kProcessing;
      return true;
    case UsedFileUploadStatus::kValidationFailed:
      *output = contextual_search::FileUploadStatus::kValidationFailed;
      return true;
    case UsedFileUploadStatus::kUploadStarted:
      *output = contextual_search::FileUploadStatus::kUploadStarted;
      return true;
    case UsedFileUploadStatus::kUploadSuccessful:
      *output = contextual_search::FileUploadStatus::kUploadSuccessful;
      return true;
    case UsedFileUploadStatus::kUploadFailed:
      *output = contextual_search::FileUploadStatus::kUploadFailed;
      return true;
    case UsedFileUploadStatus::kUploadExpired:
      *output = contextual_search::FileUploadStatus::kUploadExpired;
      return true;
    case UsedFileUploadStatus::kProcessingSuggestSignalsReady:
      *output =
          contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady;
      return true;
    case UsedFileUploadStatus::kUploadReplaced:
      *output = contextual_search::FileUploadStatus::kUploadReplaced;
      return true;
  }
  NOTREACHED();
}

// static
UsedFileUploadErrorType
EnumTraits<UsedFileUploadErrorType, contextual_search::FileUploadErrorType>::
    ToMojom(contextual_search::FileUploadErrorType input) {
  switch (input) {
    case contextual_search::FileUploadErrorType::kUnknown:
      return UsedFileUploadErrorType::kUnknown;
    case contextual_search::FileUploadErrorType::kBrowserProcessingError:
      return UsedFileUploadErrorType::kBrowserProcessingError;
    case contextual_search::FileUploadErrorType::kNetworkError:
      return UsedFileUploadErrorType::kNetworkError;
    case contextual_search::FileUploadErrorType::kServerError:
      return UsedFileUploadErrorType::kServerError;
    case contextual_search::FileUploadErrorType::kServerSizeLimitExceeded:
      return UsedFileUploadErrorType::kServerSizeLimitExceeded;
    case contextual_search::FileUploadErrorType::kAborted:
      return UsedFileUploadErrorType::kAborted;
    case contextual_search::FileUploadErrorType::kImageProcessingError:
      return UsedFileUploadErrorType::kImageProcessingError;
  }
  NOTREACHED();
}

// static
bool EnumTraits<UsedFileUploadErrorType,
                contextual_search::FileUploadErrorType>::
    FromMojom(UsedFileUploadErrorType input,
              contextual_search::FileUploadErrorType* output) {
  switch (input) {
    case UsedFileUploadErrorType::kUnknown:
      *output = contextual_search::FileUploadErrorType::kUnknown;
      return true;
    case UsedFileUploadErrorType::kBrowserProcessingError:
      *output = contextual_search::FileUploadErrorType::kBrowserProcessingError;
      return true;
    case UsedFileUploadErrorType::kNetworkError:
      *output = contextual_search::FileUploadErrorType::kNetworkError;
      return true;
    case UsedFileUploadErrorType::kServerError:
      *output = contextual_search::FileUploadErrorType::kServerError;
      return true;
    case UsedFileUploadErrorType::kServerSizeLimitExceeded:
      *output =
          contextual_search::FileUploadErrorType::kServerSizeLimitExceeded;
      return true;
    case UsedFileUploadErrorType::kAborted:
      *output = contextual_search::FileUploadErrorType::kAborted;
      return true;
    case UsedFileUploadErrorType::kImageProcessingError:
      *output = contextual_search::FileUploadErrorType::kImageProcessingError;
      return true;
  }
  NOTREACHED();
}

// static
const std::string&
StructTraits<composebox_query::mojom::AimUrlParamDataView,
             omnibox::UrlParam>::param_key(const omnibox::UrlParam& param) {
  return param.param_key();
}

// static
const std::string&
StructTraits<composebox_query::mojom::AimUrlParamDataView,
             omnibox::UrlParam>::param_value(const omnibox::UrlParam& param) {
  return param.param_value();
}

// static
bool StructTraits<
    composebox_query::mojom::AimUrlParamDataView,
    omnibox::UrlParam>::Read(composebox_query::mojom::AimUrlParamDataView data,
                             omnibox::UrlParam* output) {
  if (!data.ReadParamKey(output->mutable_param_key())) {
    return false;
  }
  if (!data.ReadParamValue(output->mutable_param_value())) {
    return false;
  }
  return true;
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
std::vector<omnibox::UrlParam>
StructTraits<UsedToolConfigDataView, omnibox::ToolConfig>::aim_url_params(
    const omnibox::ToolConfig& config) {
  return std::vector<omnibox::UrlParam>(config.aim_url_params().begin(),
                                        config.aim_url_params().end());
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

  std::vector<omnibox::UrlParam> params;
  if (!data.ReadAimUrlParams(&params)) {
    return false;
  }
  output->mutable_aim_url_params()->Clear();
  for (const auto& param : params) {
    *output->add_aim_url_params() = param;
  }

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
std::vector<omnibox::UrlParam>
StructTraits<UsedModelConfigDataView, omnibox::ModelConfig>::aim_url_params(
    const omnibox::ModelConfig& config) {
  return std::vector<omnibox::UrlParam>(config.aim_url_params().begin(),
                                        config.aim_url_params().end());
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

  std::vector<omnibox::UrlParam> params;
  if (!data.ReadAimUrlParams(&params)) {
    return false;
  }
  output->mutable_aim_url_params()->Clear();
  for (const auto& param : params) {
    *output->add_aim_url_params() = param;
  }

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
omnibox::InputType
StructTraits<UsedInputTypeConfigDataView, omnibox::InputTypeConfig>::input_type(
    const omnibox::InputTypeConfig& config) {
  return config.input_type();
}

// static
const std::string&
StructTraits<UsedInputTypeConfigDataView, omnibox::InputTypeConfig>::menu_label(
    const omnibox::InputTypeConfig& config) {
  return config.menu_label();
}

// static
bool StructTraits<UsedInputTypeConfigDataView, omnibox::InputTypeConfig>::Read(
    UsedInputTypeConfigDataView data,
    omnibox::InputTypeConfig* output) {
  omnibox::InputType input_type = omnibox::InputType::INPUT_TYPE_UNSPECIFIED;
  if (!data.ReadInputType(&input_type)) {
    return false;
  }

  output->set_input_type(input_type);

  std::string menu_label;
  if (!data.ReadMenuLabel(&menu_label)) {
    return false;
  }
  output->set_menu_label(menu_label);

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
const std::vector<omnibox::InputTypeConfig>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::input_type_configs(
    const omnibox::InputState& input) {
  return input.input_type_configs;
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
const std::map<omnibox::InputType, int>&
StructTraits<UsedInputStateDataView, omnibox::InputState>::max_instances(
    const omnibox::InputState& input) {
  return input.max_instances;
}

// static
int32_t
StructTraits<UsedInputStateDataView, omnibox::InputState>::max_total_inputs(
    const omnibox::InputState& input) {
  return input.max_total_inputs;
}

// static
bool StructTraits<UsedInputStateDataView, omnibox::InputState>::Read(
    UsedInputStateDataView data,
    omnibox::InputState* output) {
  output->max_total_inputs = data.max_total_inputs();
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
         data.ReadInputTypeConfigs(&output->input_type_configs) &&
         data.ReadToolsSectionConfig(&output->tools_section_config) &&
         data.ReadModelSectionConfig(&output->model_section_config) &&
         data.ReadHintText(&output->hint_text) &&
         data.ReadMaxInstances(&output->max_instances);
}

}  // namespace mojo
