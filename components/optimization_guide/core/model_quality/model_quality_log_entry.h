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

  virtual ~ModelQualityLogEntry();

  proto::LoggingMetadata* logging_metadata() {
    return log_ai_data_request_->mutable_logging_metadata();
  }

  int64_t client_id() const {
    return log_ai_data_request_->mutable_logging_metadata()->client_id();
  }

  template <typename FeatureType>
  FeatureType::Quality* quality_data() {
    return FeatureType::GetLoggingData(*log_ai_data_request_)
        ->mutable_quality_data();
  }

  void set_model_execution_id(const std::string& server_execution_id) {
    log_ai_data_request_->mutable_model_execution_info()
        ->set_server_execution_id(server_execution_id);
  }

  void set_error_response(const proto::ErrorResponse& error_response) {
    *(log_ai_data_request_->mutable_model_execution_info()
          ->mutable_error_response()) = error_response;
  }

  proto::LogAiDataRequest* log_ai_data_request() {
    return log_ai_data_request_.get();
  }

 private:
  friend class ModelQualityLogsUploaderService;

  // Holds feature's model execution and quality data.
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOG_ENTRY_H_
