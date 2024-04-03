// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"
namespace optimization_guide {

ModelQualityLogEntry::ModelQualityLogEntry(
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    base::WeakPtr<ModelQualityLogsUploaderService>
        model_quality_uploader_service)
    : log_ai_data_request_(std::move(log_ai_data_request)),
      model_quality_uploader_service_(model_quality_uploader_service) {}

ModelQualityLogEntry::~ModelQualityLogEntry() {
  // Upload logs if we have reference to the uploader service and the
  // LogAiDataRequest is not null. This can happen when the logs are not
  // uploaded in the feature which owns the ModelQualityLogEntry for e.g when
  // chrome is closed.
  bool uploaded_on_destruction = false;
  if (model_quality_uploader_service_ && log_ai_data_request_) {
    auto key = GetModelExecutionFeature(log_ai_data_request_->feature_case());

    if (key && model_quality_uploader_service_->CanUploadLogs(*key)) {
      // Set the system profile proto before upload. We do that here as we need
      // to access the API on //chrome.
      model_quality_uploader_service_->SetSystemProfileProto(
          logging_metadata());

      // We pass the ownership of the LogAiDataRequest to avoid re-uploading the
      // logs.
      model_quality_uploader_service_->UploadModelQualityLogs(
          std::move(log_ai_data_request_));
      uploaded_on_destruction = true;
    }
  }
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelQualityLogEntry.UploadedOnDestruction",
      uploaded_on_destruction);
}

}  // namespace optimization_guide
