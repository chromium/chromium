// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/contextual_search_mojom_traits.h"

#include "base/logging.h"
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
using UsedContextUploadStatus = composebox_query::mojom::ContextUploadStatus;
using UsedContextUploadErrorType =
    composebox_query::mojom::ContextUploadErrorType;

}  // namespace

// static
UsedToolMode EnumTraits<UsedToolMode, omnibox::ToolMode>::ToMojom(
    omnibox::ToolMode input) {
  // Guard against new, unknown values from the server cleanly.
  // This handles extensible enums securely while allowing us to omit a
  // 'default' case, preserving the compiler's -Wswitch exhaustiveness check
  // for known values.
  if (!omnibox::ToolMode_IsValid(static_cast<int>(input))) {
    return UsedToolMode::kUnspecified;
  }
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
  DUMP_WILL_BE_NOTREACHED();
  return UsedToolMode::kUnspecified;
}

// static
omnibox::ToolMode EnumTraits<UsedToolMode, omnibox::ToolMode>::FromMojom(
    UsedToolMode input) {
  switch (input) {
    case UsedToolMode::kUnspecified:
      return omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
    case UsedToolMode::kDeepSearch:
      return omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH;
    case UsedToolMode::kCanvas:
      return omnibox::ToolMode::TOOL_MODE_CANVAS;
    case UsedToolMode::kImageGen:
      return omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
    case UsedToolMode::kDeepBrowse:
      return omnibox::ToolMode::TOOL_MODE_DEEP_BROWSE;
    case UsedToolMode::kAim:
      return omnibox::ToolMode::TOOL_MODE_AIM;
    case UsedToolMode::kAimGenPrompt:
      return omnibox::ToolMode::TOOL_MODE_AIM_GEN_PROMPT;
    case UsedToolMode::kDisableSuggest:
      return omnibox::ToolMode::TOOL_MODE_DISABLE_SUGGEST;
    case UsedToolMode::kGeminiPro:
      return omnibox::ToolMode::TOOL_MODE_GEMINI_PRO;
    case UsedToolMode::kImageGenUpload:
      return omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD;
    case UsedToolMode::kImageGenSelfie:
      return omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_SELFIE;
  }
  NOTREACHED();
}

// static
UsedModelMode EnumTraits<UsedModelMode, omnibox::ModelMode>::ToMojom(
    omnibox::ModelMode input) {
  // Guard against new, unknown values from the server cleanly.
  // This handles extensible enums securely while allowing us to omit a
  // 'default' case, preserving the compiler's -Wswitch exhaustiveness check
  // for known values.
  if (!omnibox::ModelMode_IsValid(static_cast<int>(input))) {
    return UsedModelMode::kUnspecified;
  }
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
  DUMP_WILL_BE_NOTREACHED();
  return UsedModelMode::kUnspecified;
}

// static
omnibox::ModelMode EnumTraits<UsedModelMode, omnibox::ModelMode>::FromMojom(
    UsedModelMode input) {
  switch (input) {
    case UsedModelMode::kUnspecified:
      return omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
    case UsedModelMode::kGeminiRegular:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR;
    case UsedModelMode::kGeminiPro:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO;
    case UsedModelMode::kGeminiProAutoroute:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE;
    case UsedModelMode::kGeminiProNoGenUi:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;
  }
  NOTREACHED();
}

// static
UsedInputType EnumTraits<UsedInputType, omnibox::InputType>::ToMojom(
    omnibox::InputType input) {
  // Guard against new, unknown values from the server cleanly.
  // This handles extensible enums securely while allowing us to omit a
  // 'default' case, preserving the compiler's -Wswitch exhaustiveness check
  // for known values.
  if (!omnibox::InputType_IsValid(static_cast<int>(input))) {
    return UsedInputType::kUnspecified;
  }
  switch (input) {
    case omnibox::InputType::INPUT_TYPE_UNSPECIFIED:
      return UsedInputType::kUnspecified;
    case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
      return UsedInputType::kLensImage;
    case omnibox::InputType::INPUT_TYPE_LENS_FILE:
      return UsedInputType::kLensFile;
    case omnibox::InputType::INPUT_TYPE_BROWSER_TAB:
      return UsedInputType::kBrowserTab;
    case omnibox::InputType::INPUT_TYPE_DRIVE:
      return UsedInputType::kDrive;
    // The proto compiler generates these sentinel values. We must handle them
    // to satisfy the compiler's exhaustiveness check (since we don't have a
    // default case), but they should never be encountered in practice.
    case omnibox::InputType::InputType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case omnibox::InputType::InputType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  DLOG(ERROR) << "Unexpected InputType in ToMojom: " << static_cast<int>(input);
  return UsedInputType::kUnspecified;
}

// static
omnibox::InputType EnumTraits<UsedInputType, omnibox::InputType>::FromMojom(
    UsedInputType input) {
  switch (input) {
    case UsedInputType::kUnspecified:
      return omnibox::InputType::INPUT_TYPE_UNSPECIFIED;
    case UsedInputType::kLensImage:
      return omnibox::InputType::INPUT_TYPE_LENS_IMAGE;
    case UsedInputType::kLensFile:
      return omnibox::InputType::INPUT_TYPE_LENS_FILE;
    case UsedInputType::kBrowserTab:
      return omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
    case UsedInputType::kDrive:
      return omnibox::InputType::INPUT_TYPE_DRIVE;
  }
  DLOG(ERROR) << "Unexpected InputType in FromMojom: "
              << static_cast<int>(input);
  return omnibox::InputType::INPUT_TYPE_UNSPECIFIED;
}

// static
UsedContextUploadStatus
EnumTraits<UsedContextUploadStatus, contextual_search::ContextUploadStatus>::
    ToMojom(contextual_search::ContextUploadStatus input) {
  switch (input) {
    case contextual_search::ContextUploadStatus::kNotUploaded:
      return UsedContextUploadStatus::kNotUploaded;
    case contextual_search::ContextUploadStatus::kProcessing:
      return UsedContextUploadStatus::kProcessing;
    case contextual_search::ContextUploadStatus::kValidationFailed:
      return UsedContextUploadStatus::kValidationFailed;
    case contextual_search::ContextUploadStatus::kUploadStarted:
      return UsedContextUploadStatus::kUploadStarted;
    case contextual_search::ContextUploadStatus::kUploadSuccessful:
      return UsedContextUploadStatus::kUploadSuccessful;
    case contextual_search::ContextUploadStatus::kUploadFailed:
      return UsedContextUploadStatus::kUploadFailed;
    case contextual_search::ContextUploadStatus::kUploadExpired:
      return UsedContextUploadStatus::kUploadExpired;
    case contextual_search::ContextUploadStatus::kProcessingSuggestSignalsReady:
      return UsedContextUploadStatus::kProcessingSuggestSignalsReady;
    case contextual_search::ContextUploadStatus::kUploadReplaced:
      return UsedContextUploadStatus::kUploadReplaced;
  }
  return UsedContextUploadStatus::kNotUploaded;
}

// static
contextual_search::ContextUploadStatus
EnumTraits<UsedContextUploadStatus, contextual_search::ContextUploadStatus>::
    FromMojom(UsedContextUploadStatus input) {
  switch (input) {
    case UsedContextUploadStatus::kNotUploaded:
      return contextual_search::ContextUploadStatus::kNotUploaded;
    case UsedContextUploadStatus::kProcessing:
      return contextual_search::ContextUploadStatus::kProcessing;
    case UsedContextUploadStatus::kValidationFailed:
      return contextual_search::ContextUploadStatus::kValidationFailed;
    case UsedContextUploadStatus::kUploadStarted:
      return contextual_search::ContextUploadStatus::kUploadStarted;
    case UsedContextUploadStatus::kUploadSuccessful:
      return contextual_search::ContextUploadStatus::kUploadSuccessful;
    case UsedContextUploadStatus::kUploadFailed:
      return contextual_search::ContextUploadStatus::kUploadFailed;
    case UsedContextUploadStatus::kUploadExpired:
      return contextual_search::ContextUploadStatus::kUploadExpired;
    case UsedContextUploadStatus::kProcessingSuggestSignalsReady:
      return contextual_search::ContextUploadStatus::
          kProcessingSuggestSignalsReady;
    case UsedContextUploadStatus::kUploadReplaced:
      return contextual_search::ContextUploadStatus::kUploadReplaced;
  }
  NOTREACHED();
}

// static
UsedContextUploadErrorType
EnumTraits<UsedContextUploadErrorType,
           contextual_search::ContextUploadErrorType>::
    ToMojom(contextual_search::ContextUploadErrorType input) {
  switch (input) {
    case contextual_search::ContextUploadErrorType::kUnknown:
      return UsedContextUploadErrorType::kUnknown;
    case contextual_search::ContextUploadErrorType::kBrowserProcessingError:
      return UsedContextUploadErrorType::kBrowserProcessingError;
    case contextual_search::ContextUploadErrorType::kNetworkError:
      return UsedContextUploadErrorType::kNetworkError;
    case contextual_search::ContextUploadErrorType::kServerError:
      return UsedContextUploadErrorType::kServerError;
    case contextual_search::ContextUploadErrorType::kServerSizeLimitExceeded:
      return UsedContextUploadErrorType::kServerSizeLimitExceeded;
    case contextual_search::ContextUploadErrorType::kAborted:
      return UsedContextUploadErrorType::kAborted;
    case contextual_search::ContextUploadErrorType::kImageProcessingError:
      return UsedContextUploadErrorType::kImageProcessingError;
    case contextual_search::ContextUploadErrorType::
        kBrowserProcessingFileTooLargeError:
      return UsedContextUploadErrorType::kBrowserProcessingFileTooLargeError;
    case contextual_search::ContextUploadErrorType::
        kBrowserProcessingFileEmptyError:
      return UsedContextUploadErrorType::kBrowserProcessingFileEmptyError;
    case contextual_search::ContextUploadErrorType::
        kBrowserProcessingMaxFilesExceededError:
      return UsedContextUploadErrorType::
          kBrowserProcessingMaxFilesExceededError;
    case contextual_search::ContextUploadErrorType::
        kBrowserProcessingUnsupportedFileTypeError:
      return UsedContextUploadErrorType::
          kBrowserProcessingUnsupportedFileTypeError;
    case contextual_search::ContextUploadErrorType::
        kBrowserProcessingFileUploadNotAllowedError:
      return UsedContextUploadErrorType::
          kBrowserProcessingFileUploadNotAllowedError;
    case contextual_search::ContextUploadErrorType::
        kBrowserProcessingMaxImagesExceededError:
      return UsedContextUploadErrorType::
          kBrowserProcessingMaxImagesExceededError;
    case contextual_search::ContextUploadErrorType::
        kBrowserProcessingMaxPdfsExceededError:
      return UsedContextUploadErrorType::kBrowserProcessingMaxPdfsExceededError;
  }
  return UsedContextUploadErrorType::kUnknown;
}

// static
contextual_search::ContextUploadErrorType
EnumTraits<UsedContextUploadErrorType,
           contextual_search::ContextUploadErrorType>::
    FromMojom(UsedContextUploadErrorType input) {
  switch (input) {
    case UsedContextUploadErrorType::kUnknown:
      return contextual_search::ContextUploadErrorType::kUnknown;
    case UsedContextUploadErrorType::kBrowserProcessingError:
      return contextual_search::ContextUploadErrorType::kBrowserProcessingError;
    case UsedContextUploadErrorType::kNetworkError:
      return contextual_search::ContextUploadErrorType::kNetworkError;
    case UsedContextUploadErrorType::kServerError:
      return contextual_search::ContextUploadErrorType::kServerError;
    case UsedContextUploadErrorType::kServerSizeLimitExceeded:
      return contextual_search::ContextUploadErrorType::
          kServerSizeLimitExceeded;
    case UsedContextUploadErrorType::kAborted:
      return contextual_search::ContextUploadErrorType::kAborted;
    case UsedContextUploadErrorType::kImageProcessingError:
      return contextual_search::ContextUploadErrorType::kImageProcessingError;
    case UsedContextUploadErrorType::kBrowserProcessingFileTooLargeError:
      return contextual_search::ContextUploadErrorType::
          kBrowserProcessingFileTooLargeError;
    case UsedContextUploadErrorType::kBrowserProcessingFileEmptyError:
      return contextual_search::ContextUploadErrorType::
          kBrowserProcessingFileEmptyError;
    case UsedContextUploadErrorType::kBrowserProcessingMaxFilesExceededError:
      return contextual_search::ContextUploadErrorType::
          kBrowserProcessingMaxFilesExceededError;
    case UsedContextUploadErrorType::kBrowserProcessingUnsupportedFileTypeError:
      return contextual_search::ContextUploadErrorType::
          kBrowserProcessingUnsupportedFileTypeError;
    case UsedContextUploadErrorType::
        kBrowserProcessingFileUploadNotAllowedError:
      return contextual_search::ContextUploadErrorType::
          kBrowserProcessingFileUploadNotAllowedError;
    case UsedContextUploadErrorType::kBrowserProcessingMaxImagesExceededError:
      return contextual_search::ContextUploadErrorType::
          kBrowserProcessingMaxImagesExceededError;
    case UsedContextUploadErrorType::kBrowserProcessingMaxPdfsExceededError:
      return contextual_search::ContextUploadErrorType::
          kBrowserProcessingMaxPdfsExceededError;
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
StructTraits<UsedInputStateDataView, omnibox::InputState>::max_inputs_by_type(
    const omnibox::InputState& input) {
  return input.max_inputs_by_type;
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
         data.ReadMaxInputsByType(&output->max_inputs_by_type);
}

}  // namespace mojo
