// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLIPBOARD_ANALYSIS_REQUEST_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLIPBOARD_ANALYSIS_REQUEST_H_

#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"

namespace enterprise_connectors {

// A `BinaryUploadService::Request` implementation that gets the data to scan
// from a string corresponding to some clipboard data (either text or image
// bytes).
class ClipboardAnalysisRequest : public BinaryUploadRequest {
 public:
  ClipboardAnalysisRequest(
      CloudOrLocalAnalysisSettings settings,
      std::string text,
      BinaryUploadRequest::ContentAnalysisCallback callback,
      BrowserPolicyConnectorGetter policy_connector_getter);
  ~ClipboardAnalysisRequest() override;

  ClipboardAnalysisRequest(const ClipboardAnalysisRequest&) = delete;
  ClipboardAnalysisRequest& operator=(const ClipboardAnalysisRequest&) = delete;

  // BinaryUploadRequest
  void GetRequestData(DataCallback callback) override;

 private:
  Data data_;
  ScanRequestUploadResult result_ = ScanRequestUploadResult::kFileTooLarge;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLIPBOARD_ANALYSIS_REQUEST_H_
