// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_util.h"

#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/prefs/pref_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

const char kModelOverrideSeparator[] = "|";

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's request data.
void SetExecutionRequest(
    ModelBasedCapabilityKey feature,
    proto::LogAiDataRequest& log_ai_request,
    const google::protobuf::MessageLite& request_metadata) {
  switch (feature) {
    case ModelBasedCapabilityKey::kWallpaperSearch:
      SetExecutionRequestTemplate<WallpaperSearchFeatureTypeMap>(
          log_ai_request, request_metadata);
      return;
    case ModelBasedCapabilityKey::kTabOrganization:
      SetExecutionRequestTemplate<TabOrganizationFeatureTypeMap>(
          log_ai_request, request_metadata);
      return;
    case ModelBasedCapabilityKey::kCompose:
      SetExecutionRequestTemplate<ComposeFeatureTypeMap>(log_ai_request,
                                                         request_metadata);
      return;
    case ModelBasedCapabilityKey::kTextSafety:
    case ModelBasedCapabilityKey::kTest:
      // Do not log request for test and text safety.
      return;
  }
}

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's response data.
void SetExecutionResponse(ModelBasedCapabilityKey feature,
                          proto::LogAiDataRequest& log_ai_request,
                          const proto::Any& response_metadata) {
  switch (feature) {
    case ModelBasedCapabilityKey::kWallpaperSearch:
      SetExecutionResponseTemplate<WallpaperSearchFeatureTypeMap>(
          log_ai_request, response_metadata);
      return;
    case ModelBasedCapabilityKey::kTabOrganization:
      SetExecutionResponseTemplate<TabOrganizationFeatureTypeMap>(
          log_ai_request, response_metadata);
      return;
    case ModelBasedCapabilityKey::kCompose:
      SetExecutionResponseTemplate<ComposeFeatureTypeMap>(log_ai_request,
                                                          response_metadata);
      return;
    case ModelBasedCapabilityKey::kTextSafety:
    case ModelBasedCapabilityKey::kTest:
      // Do not log response for test and text safety.
      return;
  }
}

prefs::GenAILocalFoundationalModelEnterprisePolicySettings
GetGenAILocalFoundationalModelEnterprisePolicySettings(
    PrefService* local_state) {
  return static_cast<
      prefs::GenAILocalFoundationalModelEnterprisePolicySettings>(
      local_state->GetInteger(
          prefs::localstate::
              kGenAILocalFoundationalModelEnterprisePolicySettings));
}

std::optional<on_device_model::AdaptationAssetPaths>
GetOnDeviceModelAdaptationOverride(proto::ModelExecutionFeature feature) {
  auto adaptations_override_switch =
      switches::GetOnDeviceModelAdaptationsOverride();
  if (!adaptations_override_switch) {
    return std::nullopt;
  }

  for (const auto& adaptation_override :
       base::SplitString(*adaptations_override_switch, ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> override_parts =
        base::SplitString(adaptation_override, kModelOverrideSeparator,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (override_parts.size() != 2 && override_parts.size() != 3) {
      // Input is malformed.
      DLOG(ERROR) << "Invalid string format provided to the on-device model "
                     "adaptations override";
      return std::nullopt;
    }
    proto::ModelExecutionFeature this_feature;
    if (!proto::ModelExecutionFeature_Parse(override_parts[0], &this_feature)) {
      DLOG(ERROR) << "Invalid optimization target provided to the on-device "
                     "model adaptations override";
      return std::nullopt;
    }
    if (feature != this_feature) {
      continue;
    }
    on_device_model::AdaptationAssetPaths adaptation_asset;
    adaptation_asset.weights = *StringToFilePath(override_parts[1]);
    if (!adaptation_asset.weights.IsAbsolute()) {
      DLOG(ERROR)
          << "Provided model adaptations weights file path must be absolute "
          << adaptation_asset.weights;
      return std::nullopt;
    }
    if (override_parts.size() == 3) {
      adaptation_asset.model = *StringToFilePath(override_parts[2]);
      if (!adaptation_asset.model.IsAbsolute()) {
        DLOG(ERROR) << "Provided model adaptations file path must be absolute "
                    << adaptation_asset.model;
        return std::nullopt;
      }
    }
    return adaptation_asset;
  }
  return std::nullopt;
}

OnDeviceModelLoadResult ConvertToOnDeviceModelLoadResult(
    on_device_model::mojom::LoadModelResult result) {
  switch (result) {
    case on_device_model::mojom::LoadModelResult::kSuccess:
      return OnDeviceModelLoadResult::kSuccess;
    case on_device_model::mojom::LoadModelResult::kGpuBlocked:
      return OnDeviceModelLoadResult::kGpuBlocked;
    case on_device_model::mojom::LoadModelResult::kFailedToLoadLibrary:
      return OnDeviceModelLoadResult::kFailedToLoadLibrary;
  }
}

}  // namespace optimization_guide
