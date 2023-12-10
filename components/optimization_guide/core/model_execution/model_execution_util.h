// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_

#include <memory>

#include "base/check.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

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
  *(logging_data->mutable_request_data()) = typed_request;
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
  *(logging_data->mutable_response_data()) = std::move(*response_data);
  CHECK(logging_data->has_response_data()) << "Response data is not set\n";
}

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's request data.
void SetExecutionRequest(proto::ModelExecutionFeature feature,
                         proto::LogAiDataRequest& log_ai_request,
                         const google::protobuf::MessageLite& request_metadata);

// Helper method matches feature to corresponding FeatureTypeMap to set
// LogAiDataRequest's response data.
void SetExecutionResponse(proto::ModelExecutionFeature feature,
                          proto::LogAiDataRequest& log_ai_request,
                          const proto::Any& response_metadata);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_UTIL_H_
