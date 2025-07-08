// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"

#include "base/base64.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"

namespace {

void SetMqlsLogForWebUI(
    base::WeakPtr<optimization_guide::ModelQualityLogsUploaderService> uploader,
    std::string feature,
    std::string proto,
    std::string status) {
  if (!uploader) {
    return;
  }

  auto mqls_log_ptr =
      optimization_guide_internals::mojom::MqlsLog::New(feature, proto, status);
  uploader->SetMqlsLogForWebUI(std::move(mqls_log_ptr));
}

}  // namespace

namespace optimization_guide {

ModelQualityLogEntry::ModelQualityLogEntry(
    base::WeakPtr<ModelQualityLogsUploaderService> uploader)
    : log_ai_data_request_(std::make_unique<proto::LogAiDataRequest>()),
      uploader_(uploader) {}

ModelQualityLogEntry::~ModelQualityLogEntry() {
  // Upload logs upon destruction. Typical usage will destroy a log entry
  // intentionally in order to trigger upload. However, uploading upon
  // destruction also covers the case when the logs are not explicitly uploaded
  // in the feature code -- for example, when Chrome is closed.

  // Bail early if there's nothing to upload. The uploader will not exist if
  // uploading is not allowed -- for example, in Incognito mode.
  if (!log_ai_data_request_) {
    return;
  }
  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          log_ai_data_request_->feature_case());
  std::string serialized_proto =
      base::Base64Encode(log_ai_data_request_->SerializeAsString());
  if (!metadata) {
    // The feature is not configured to use MQLS, don't upload anything.
    SetMqlsLogForWebUI(uploader_,
                       absl::StrFormat("Feature case: %d",
                                       log_ai_data_request_->feature_case()),
                       serialized_proto, "Feature not configured to use MQLS");
    return;
  }
  if (!uploader_ || !uploader_->CanUploadLogs(metadata)) {
    SetMqlsLogForWebUI(uploader_, metadata->name(), serialized_proto,
                       "Not allowed to upload");
    return;
  }

  SetMqlsLogForWebUI(uploader_, metadata->name(), serialized_proto,
                     "Sent for upload");
  uploader_->UploadModelQualityLogs(std::move(log_ai_data_request_));
}

// static
void ModelQualityLogEntry::Upload(std::unique_ptr<ModelQualityLogEntry> entry) {
  if (entry) {
    // Destroying the log entry triggers an upload.
    entry.reset();
  }
}

// static
void ModelQualityLogEntry::Drop(std::unique_ptr<ModelQualityLogEntry> entry) {
  if (entry) {
    // Clearing the data results in dropping the log.
    entry->log_ai_data_request_.reset();
  }
}

}  // namespace optimization_guide
