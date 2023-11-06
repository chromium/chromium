// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOG_ENTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOG_ENTRY_H_

#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

// Contains all the information required for building LogAiDataRequest to sent
// to model quality service to log data. Lifetime of ModelQualityLogEntry is
// bound to the caller which creates it and gets deleted when the logs have been
// uploaded to the model quality service.
class ModelQualityLogEntry {
 public:
  explicit ModelQualityLogEntry(
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request);

  ~ModelQualityLogEntry();

  proto::LoggingMetadata* logging_metadata() {
    return log_ai_data_request_.get()->mutable_logging_metadata();
  }

  template <typename FeatureType>
  FeatureType::ModelQualityData* quality_data() {
    return FeatureType::GetLoggingData(log_ai_data_request_)
        .mutable_quality_data();
  }

 private:
  // Holds feature's model execution and quality data.
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOG_ENTRY_H_
