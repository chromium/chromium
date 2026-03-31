// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"

#include "components/enterprise/connectors/core/common.h"

namespace enterprise_connectors {

FilesRequestHandlerBase::FilesRequestHandlerBase(
    ContentAnalysisInfoBase* content_analysis_info,
    BinaryUploadService* upload_service,
    GURL url,
    DeepScanAccessPoint access_point,
    std::unique_ptr<FilesRequestHandlerBase::Delegate> delegate)
    : RequestHandlerBase(content_analysis_info,
                         upload_service,
                         url,
                         access_point),
      delegate_(std::move(delegate)) {}

FilesRequestHandlerBase::~FilesRequestHandlerBase() = default;

void FilesRequestHandlerBase::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  delegate_->ReportWarningBypass(user_justification);
}

bool FilesRequestHandlerBase::UploadDataImpl() {
  return delegate_->UploadDataImpl();
}

}  // namespace enterprise_connectors
