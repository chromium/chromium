// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_util.h"

#include "base/files/file_util.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/prefs/pref_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

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
    case ModelBasedCapabilityKey::kPromptApi:
    case ModelBasedCapabilityKey::kTextSafety:
    case ModelBasedCapabilityKey::kTest:
      // Do not log requests for these features.
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
    case ModelBasedCapabilityKey::kPromptApi:
    case ModelBasedCapabilityKey::kTextSafety:
    case ModelBasedCapabilityKey::kTest:
      // Do not log responses for these features.
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

std::unique_ptr<proto::OnDeviceModelExecutionConfig>
ReadOnDeviceModelExecutionConfig(const base::FilePath& config_path) {
  // Unpack and verify model config file.
  std::string binary_config_pb;
  if (!base::ReadFileToString(config_path, &binary_config_pb)) {
    return nullptr;
  }

  auto config = std::make_unique<proto::OnDeviceModelExecutionConfig>();
  if (!config->ParseFromString(binary_config_pb)) {
    return nullptr;
  }
  return config;
}

}  // namespace optimization_guide
