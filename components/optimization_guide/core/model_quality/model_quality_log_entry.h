// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOG_ENTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOG_ENTRY_H_

#include <cstdint>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

// Contains all the information required for building LogAiDataRequest to sent
// to model quality service to log data. Lifetime of ModelQualityLogEntry is
// bound to the caller which creates it and gets deleted when the logs have been
// uploaded to the model quality service.
class ModelQualityLogEntry {
 public:
  explicit ModelQualityLogEntry(
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      base::WeakPtr<ModelQualityLogsUploaderService> uploader);

  virtual ~ModelQualityLogEntry();

  // Uploads the log entry if uploading is allowed; otherwise drops the entry.
  // Note that the ModelQualityLogsEntry class will upload logs upon
  // destruction, so most clients can skip calling this method.
  static void Upload(std::unique_ptr<ModelQualityLogEntry> entry);

  // Drops the passed log entry, to ensure that it is never uploaded.
  // Useful, for example, if the content of the log entry is stale.
  static void Drop(std::unique_ptr<ModelQualityLogEntry> entry);

  template <typename FeatureType>
  FeatureType::Quality* quality_data() {
    return FeatureType::GetLoggingData(*log_ai_data_request_)
        ->mutable_quality();
  }

  void set_model_execution_id(const std::string& execution_id) {
    log_ai_data_request_->mutable_model_execution_info()->set_execution_id(
        execution_id);
  }
  std::string model_execution_id() const {
    return log_ai_data_request_->model_execution_info().execution_id();
  }

  void set_error_response(const proto::ErrorResponse& error_response) {
    *(log_ai_data_request_->mutable_model_execution_info()
          ->mutable_error_response()) = error_response;
  }

  void set_model_execution_error(
      const OptimizationGuideModelExecutionError& model_execution_error) {
    log_ai_data_request_->mutable_model_execution_info()
        ->set_model_execution_error_enum(
            static_cast<uint32_t>(model_execution_error.error()));
  }

  proto::LogAiDataRequest* log_ai_data_request() {
    return log_ai_data_request_.get();
  }

 private:
  friend class ModelQualityLogsUploaderServiceTest;

  // Holds feature's model execution and quality data.
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request_;

  // The ModelQualityLogsUploaderService used to upload logs.
  base::WeakPtr<ModelQualityLogsUploaderService> uploader_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOG_ENTRY_H_
