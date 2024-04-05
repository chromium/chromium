// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_util.h"

#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/prefs/pref_service.h"

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

}  // namespace optimization_guide
