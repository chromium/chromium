// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model_routing.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/input_state.h"
#include "third_party/omnibox_proto/url_param.pb.h"

namespace contextual_search {

namespace {

// TODO(crbug.com/579428915): Share the param constants and the config lookups
// below with `InputStateModel`.
constexpr char kUdmKey[] = "udm";
constexpr char kNemKey[] = "nem";
constexpr char kAimUdmValue[] = "50";

// Returns whether `params` select AIM: `udm=50` or any `nem`.
bool ParamsRouteToAim(const UrlParams& params) {
  if (params.contains(kNemKey)) {
    return true;
  }
  auto it = params.find(kUdmKey);
  return it != params.end() && it->second == kAimUdmValue;
}

// Adds `to_merge` to `params`, replacing params with the same key.
void MergeParams(
    UrlParams& params,
    const google::protobuf::RepeatedPtrField<omnibox::UrlParam>& to_merge) {
  for (const auto& param : to_merge) {
    params[param.param_key()] = param.param_value();
  }
}

// Returns the config for `tool`, or null if there's none.
const omnibox::ToolConfig* FindToolConfig(const omnibox::InputState& state,
                                          omnibox::ToolMode tool) {
  auto it =
      std::ranges::find(state.tool_configs, tool, &omnibox::ToolConfig::tool);
  return it != state.tool_configs.end() ? &*it : nullptr;
}

// Returns the config for `model`, or null if there's none.
const omnibox::ModelConfig* FindModelConfig(const omnibox::InputState& state,
                                            omnibox::ModelMode model) {
  auto it = std::ranges::find(state.model_configs, model,
                              &omnibox::ModelConfig::model);
  return it != state.model_configs.end() ? &*it : nullptr;
}

}  // namespace

FulfillmentDecision::FulfillmentDecision() = default;
FulfillmentDecision::FulfillmentDecision(const FulfillmentDecision&) = default;
FulfillmentDecision& FulfillmentDecision::operator=(
    const FulfillmentDecision&) = default;
FulfillmentDecision::FulfillmentDecision(FulfillmentDecision&&) noexcept =
    default;
FulfillmentDecision& FulfillmentDecision::operator=(
    FulfillmentDecision&&) noexcept = default;
FulfillmentDecision::~FulfillmentDecision() = default;

SrpEligibleAttachments EligibleAttachmentsForMode(SrpInputMode mode) {
  switch (mode) {
    case SrpInputMode::kNone:
      return {};
    case SrpInputMode::kSingleImage:
      return {.single_image = true};
    case SrpInputMode::kMultiImage:
      return {.single_image = true, .multi_image = true};
    case SrpInputMode::kAnyCombination:
      return {
          .single_image = true, .multi_image = true, .any_combination = true};
  }
  NOTREACHED();
}

std::optional<SrpInputMode> ParseSrpInputMode(std::string_view name) {
  if (name == "none") {
    return SrpInputMode::kNone;
  }
  if (name == "single_image") {
    return SrpInputMode::kSingleImage;
  }
  if (name == "multi_image") {
    return SrpInputMode::kMultiImage;
  }
  if (name == "any_combination") {
    return SrpInputMode::kAnyCombination;
  }
  return std::nullopt;
}

SrpInputMode SrpInputModeFromFeatureParams() {
  return ParseSrpInputMode(omnibox::kDynamicAimFulfillmentSrpInputMode.Get())
      .value_or(SrpInputMode::kSingleImage);
}

AttachmentCounts CountAttachments(const std::vector<FileInfo>& file_infos,
                                  size_t pending_tab_count) {
  AttachmentCounts counts;
  for (const auto& file_info : file_infos) {
    if (file_info.is_superceded) {
      continue;
    }
    if (file_info.tab_url.has_value()) {
      ++counts.tabs;
    } else if (file_info.input_data &&
               file_info.input_data->drive_id.has_value()) {
      ++counts.drive;
    } else if (file_info.mime_type == lens::MimeType::kImage) {
      ++counts.images;
    } else {
      ++counts.files;
    }
  }
  counts.tabs += base::checked_cast<int>(pending_tab_count);
  return counts;
}

bool AttachmentsCanGoToSrp(const AttachmentCounts& attachments,
                           const SrpEligibleAttachments& eligible) {
  if (attachments.tabs > 0 || attachments.drive > 0) {
    return false;
  }
  if (attachments.images == 0 && attachments.files == 0) {
    return true;
  }
  if (attachments.files > 0) {
    return eligible.any_combination;
  }
  if (attachments.images > 1) {
    return eligible.multi_image;
  }
  return eligible.single_image;
}

FulfillmentDecision ComputeFulfillment(const omnibox::InputState& state,
                                       bool aim_mode_active,
                                       const AttachmentCounts& attachments,
                                       const SrpEligibleAttachments& eligible,
                                       bool has_valid_config) {
  const bool has_tool =
      state.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  const omnibox::ToolConfig* tool_config =
      has_tool ? FindToolConfig(state, state.active_tool) : nullptr;
  UrlParams params;
  if (tool_config) {
    MergeParams(params, tool_config->aim_url_params());
  }

  // A tool goes by its params, or to AIM if it has no config.
  const bool tool_needs_aim =
      has_tool && (!tool_config || ParamsRouteToAim(params));
  // A tool with attachments (e.g. editing an image) isn't currently supported
  // by AIO on SRP, so it goes to AIM.
  const bool tool_with_attachments =
      has_tool && attachments != AttachmentCounts();
  const bool attachments_need_aim =
      !AttachmentsCanGoToSrp(attachments, eligible);

  FulfillmentDecision decision;
  decision.will_fulfill_in_aim =
      !has_valid_config ||
      // Whether AI Mode was activated by the current composition.
      aim_mode_active || tool_needs_aim || tool_with_attachments ||
      attachments_need_aim;
  if (!decision.will_fulfill_in_aim) {
    decision.url_params = std::move(params);
    return decision;
  }

  // Use the chosen model if it's still allowed, otherwise the default.
  decision.model =
      std::ranges::contains(state.allowed_models, state.active_model)
          ? state.active_model
          : state.GetDefaultModel();
  if (const omnibox::ModelConfig* model_config =
          FindModelConfig(state, decision.model)) {
    MergeParams(params, model_config->aim_url_params());
  }
  // AIM needs `udm=50`, or `nem` instead of `udm`.
  if (params.contains(kNemKey)) {
    params.erase(kUdmKey);
  } else {
    params[kUdmKey] = kAimUdmValue;
  }
  decision.url_params = std::move(params);
  return decision;
}

}  // namespace contextual_search
