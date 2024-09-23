// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_

#include <memory>

#include "base/check.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

// Sets request data corresponding the feature's LogAiDataRequest.
template <typename FeatureType>
void SetExecutionRequestTemplate(
    proto::LogAiDataRequest& log_ai_request,
    const google::protobuf::MessageLite& request_metadata) {
  typename FeatureType::LoggingData* logging_data =
      FeatureType::GetLoggingData(log_ai_request);
  CHECK(logging_data);

  // Request is set by the feature and should always be typed.
  auto typed_request =
      static_cast<const FeatureType::Request&>(request_metadata);
  *(logging_data->mutable_request()) = typed_request;
}

// Sets response data corresponding the feature's LogAiDataRequest.
template <typename FeatureType>
void SetExecutionResponseTemplate(proto::LogAiDataRequest& log_ai_request,
                                  const proto::Any& response_metadata) {
  typename FeatureType::LoggingData* logging_data =
      FeatureType::GetLoggingData(log_ai_request);
  CHECK(logging_data);

  // Deserialize any to correct feature response type.
  auto response_data =
      optimization_guide::ParsedAnyMetadata<typename FeatureType::Response>(
          response_metadata);

  if (!response_data) {
    return;
  }

  // Set the response data to feature LoggingData if exists.
  *(logging_data->mutable_response()) = std::move(*response_data);
  CHECK(logging_data->has_response()) << "Response data is not set\n";
}

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's request data.
void SetExecutionRequest(ModelBasedCapabilityKey feature,
                         proto::LogAiDataRequest& log_ai_request,
                         const google::protobuf::MessageLite& request_metadata);

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's response data.
void SetExecutionResponse(ModelBasedCapabilityKey feature,
                          proto::LogAiDataRequest& log_ai_request,
                          const proto::Any& response_metadata);

// Returns the GenAILocalFoundationalModelEnterprisePolicySettings from the
// `local_state`.
model_execution::prefs::GenAILocalFoundationalModelEnterprisePolicySettings
GetGenAILocalFoundationalModelEnterprisePolicySettings(
    PrefService* local_state);

OnDeviceModelLoadResult ConvertToOnDeviceModelLoadResult(
    on_device_model::mojom::LoadModelResult result);

// Returns the model execution config read from the `config_path`.
std::unique_ptr<proto::OnDeviceModelExecutionConfig>
ReadOnDeviceModelExecutionConfig(const base::FilePath& config_path);

// Returns whether the `feature` was recently used.
bool WasOnDeviceEligibleFeatureRecentlyUsed(ModelBasedCapabilityKey feature,
                                            const PrefService& local_state);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_
