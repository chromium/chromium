// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_UTIL_H_

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

// Returns ModelExecutionFeature corresponding to the
// proto::LogAiDataRequest::FeatureCase.
std::optional<UserVisibleFeatureKey> GetModelExecutionFeature(
    proto::LogAiDataRequest::FeatureCase feature);

// Helper method to get the quality_data from `log_ai_data_request` for
// different features.
template <typename FeatureType>
FeatureType::Quality* GetModelQualityData(
    proto::LogAiDataRequest* log_ai_data_request) {
  return FeatureType::GetLoggingData(*log_ai_data_request)
      ->mutable_quality_data();
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_UTIL_H_
