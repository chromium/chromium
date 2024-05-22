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
    base::WeakPtr<ModelQualityLogsUploaderService> uploader)
    : log_ai_data_request_(std::move(log_ai_data_request)),
      uploader_(uploader) {}

ModelQualityLogEntry::~ModelQualityLogEntry() {
  // Upload logs if we have reference to the uploader service and the
  // LogAiDataRequest is not null. This can happen when the logs are not
  // uploaded in the feature which owns the ModelQualityLogEntry for e.g when
  // chrome is closed.
  bool uploaded_on_destruction = false;
  if (uploader_ && log_ai_data_request_) {
    auto key = GetModelExecutionFeature(log_ai_data_request_->feature_case());

    if (key && uploader_->CanUploadLogs(*key)) {
      // Set the system profile proto before upload. We do that here as we need
      // to access the API on //chrome.
      uploader_->SetSystemProfileProto(
          log_ai_data_request_->mutable_logging_metadata());

      // We pass the ownership of the LogAiDataRequest to avoid re-uploading the
      // logs.
      uploader_->UploadModelQualityLogs(std::move(log_ai_data_request_));
      uploaded_on_destruction = true;
    }
  }
  base::UmaHistogramBoolean(
      "OptimizationGuide.ModelQualityLogEntry.UploadedOnDestruction",
      uploaded_on_destruction);
}

// static
void ModelQualityLogEntry::Upload(std::unique_ptr<ModelQualityLogEntry> entry) {
  // Destroying the log entry triggers an upload.
  entry.reset();
}

// static
void ModelQualityLogEntry::Drop(std::unique_ptr<ModelQualityLogEntry> entry) {
  // Clearing the data results in dropping the log.
  entry->log_ai_data_request_.reset();
}

}  // namespace optimization_guide
