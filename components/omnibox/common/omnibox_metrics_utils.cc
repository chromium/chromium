// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_metrics_utils.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

namespace omnibox {

// LINT.IfChange(ToolMode)
std::string GetToolModeString(omnibox::ToolMode mode) {
  switch (mode) {
    case omnibox::TOOL_MODE_UNSPECIFIED:
      return "Unspecified";
    case omnibox::TOOL_MODE_DEEP_SEARCH:
      return "DeepSearch";
    case omnibox::TOOL_MODE_CANVAS:
      return "Canvas";
    case omnibox::TOOL_MODE_GEMINI_PRO:
      return "GeminiPro";
    case omnibox::TOOL_MODE_IMAGE_GEN:
      return "ImageGen";
    case omnibox::TOOL_MODE_DEEP_BROWSE:
      return "DeepBrowse";
    case omnibox::TOOL_MODE_IMAGE_GEN_SELFIE:
      return "ImageGenSelfie";
    case omnibox::TOOL_MODE_IMAGE_GEN_UPLOAD:
      return "ImageGenUpload";
    case omnibox::TOOL_MODE_DISABLE_SUGGEST:
      return "DisableSuggest";
    case omnibox::TOOL_MODE_AIM:
      return "Aim";
    case omnibox::TOOL_MODE_AIM_GEN_PROMPT:
      return "AimGenPrompt";
    default:
      return "Unspecified";
  }
}
// LINT.ThenChange(//third_party/omnibox_proto/tool_mode.proto:ToolMode,
// //tools/metrics/histograms/metadata/omnibox/enums.xml:OmniboxToolMode)

// LINT.IfChange(ModelMode)
std::string GetModelModeString(omnibox::ModelMode mode) {
  switch (mode) {
    case omnibox::MODEL_MODE_UNSPECIFIED:
      return "Unspecified";
    case omnibox::MODEL_MODE_GEMINI_REGULAR:
      return "GeminiRegular";
    case omnibox::MODEL_MODE_GEMINI_PRO:
      return "GeminiPro";
    case omnibox::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return "GeminiProAutoroute";
    case omnibox::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
      return "GeminiProNoGenUi";
    default:
      return "Unspecified";
  }
}
// LINT.ThenChange(//third_party/omnibox_proto/model_mode.proto:ModelMode,
// //tools/metrics/histograms/metadata/omnibox/enums.xml:OmniboxModelMode)

// LINT.IfChange(GetContextTypeString)
std::string GetContextTypeString(ContextType type) {
  switch (type) {
    case ContextType::kTab:
      return "Tab";
    case ContextType::kFile:
      return "File";
    case ContextType::kImage:
      return "Image";
    case ContextType::kImageGen:
      return "ImageGen";
    case ContextType::kDeepResearch:
      return "DeepResearch";
    case ContextType::kDrive:
      return "Drive";
    case ContextType::kCanvas:
      return "Canvas";
    case ContextType::kAutoModel:
      return "AutoModel";
    case ContextType::kThinkingModel:
      return "ThinkingModel";
    case ContextType::kRegularModel:
      return "RegularModel";
    case ContextType::kProNoGenUiModel:
      return "ProNoGenUiModel";
    case ContextType::kUnknown:
      return "Unknown";
  }
}
// LINT.ThenChange(//ui/webui/resources/cr_components/composebox/common.ts:getContextTypeString)

void LogResultToContentReadyEarlyExitReason(
    ResultToContentReadyEarlyExitReason reason) {
  std::string_view name = "Omnibox.Popup.ResultToContentReadyEarlyExitReason";
  base::UmaHistogramEnumeration(name, reason);
}

}  // namespace omnibox
