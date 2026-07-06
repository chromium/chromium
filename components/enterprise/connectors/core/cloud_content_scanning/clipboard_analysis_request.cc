// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/clipboard_analysis_request.h"

#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"

namespace enterprise_connectors {

ClipboardAnalysisRequest::ClipboardAnalysisRequest(
    CloudOrLocalAnalysisSettings settings,
    std::string text,
    BinaryUploadRequest::ContentAnalysisCallback callback,
    BrowserPolicyConnectorGetter policy_connector_getter)
    : BinaryUploadRequest(std::move(callback),
                          std::move(settings),
                          std::move(policy_connector_getter)) {
  DCHECK_GT(text.size(), 0u);
  data_.size = text.size();

  // Only remember strings less than the maximum allowed.
  if (text.size() < BinaryUploadService::kMaxUploadSizeBytes) {
    data_.contents = std::move(text);
    result_ = ScanRequestUploadResult::kSuccess;
  }
  IncrementCrashKey(ScanningCrashKey::PENDING_TEXT_UPLOADS);
  IncrementCrashKey(ScanningCrashKey::TOTAL_TEXT_UPLOADS);
}

ClipboardAnalysisRequest::~ClipboardAnalysisRequest() {
  DecrementCrashKey(ScanningCrashKey::PENDING_TEXT_UPLOADS);
}

void ClipboardAnalysisRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(result_, data_);
}

}  // namespace enterprise_connectors
