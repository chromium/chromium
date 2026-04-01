// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"

#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/common.h"

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#endif

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

void FilesRequestHandlerBase::OnGotFileInfo(
    std::unique_ptr<BinaryUploadRequest> request,
    size_t index,
    ScanRequestUploadResult result,
    BinaryUploadRequest::Data data) {
  delegate_->UpdateFileInfo(index, data);

  const auto& analysis_settings = content_analysis_info_->settings();
  bool is_cloud = analysis_settings.cloud_or_local_settings.is_cloud_analysis();
  bool is_resumable = IsResumableUpload(*request);
  bool failed = is_resumable
                    ? CloudResumableResultIsFailure(
                          result, analysis_settings.block_large_files,
                          analysis_settings.block_password_protected_files)
                    : (is_cloud ? CloudMultipartResultIsFailure(result)
                                : LocalResultIsFailure(result));

  if (failed) {
    FinishRequestEarly(std::move(request), result);
    return;
  }

  // Don't bother sending empty files for deep scanning.
  if (data.size == 0) {
    FinishRequestEarly(std::move(request), ScanRequestUploadResult::kSuccess);
    return;
  }

  // If |throttled_| is true, then the file shouldn't be upload since the server
  // is receiving too many requests.
  if (throttled_) {
    FinishRequestEarly(std::move(request),
                       ScanRequestUploadResult::kTooManyRequests);
    return;
  }

  UploadFileForDeepScanning(result, std::move(request));
}

void FilesRequestHandlerBase::FinishRequestEarly(
    std::unique_ptr<BinaryUploadRequest> request,
    ScanRequestUploadResult result) {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanRequests(
          request->per_profile_request(),
          /*access_token*/ "",
          /*upload_info*/ ScanRequestUploadResultToString(result),
          /*upload_url=*/"", request->content_analysis_request());
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanResponses(
          /*token=*/"", ScanRequestUploadResultToString(result),
          enterprise_connectors::ContentAnalysisResponse());
#endif

  request->FinishRequest(result,
                         enterprise_connectors::ContentAnalysisResponse());
}

void FilesRequestHandlerBase::UploadFileForDeepScanning(
    ScanRequestUploadResult result,
    std::unique_ptr<BinaryUploadRequest> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service) {
    upload_service->MaybeUploadForDeepScanning(std::move(request));
  }
}

}  // namespace enterprise_connectors
