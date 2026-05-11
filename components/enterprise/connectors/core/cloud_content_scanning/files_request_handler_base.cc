// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"

#include "base/metrics/histogram_functions.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"

#if !BUILDFLAG(IS_IOS)
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#endif

namespace enterprise_connectors {

namespace {

AnalysisConnector AccessPointToEnterpriseConnector(
    DeepScanAccessPoint access_point) {
  switch (access_point) {
    case DeepScanAccessPoint::FILE_TRANSFER:
      return enterprise_connectors::FILE_TRANSFER;
    case DeepScanAccessPoint::UPLOAD:
    case DeepScanAccessPoint::DRAG_AND_DROP:
    case DeepScanAccessPoint::PASTE:
    case DeepScanAccessPoint::ACTOR:
      // A file can be uploaded to a website by either a normal file picker, a
      // dragNdrop event, using copy+paste, or an agent action.
      return enterprise_connectors::FILE_ATTACHED;
    case DeepScanAccessPoint::DOWNLOAD:
      return enterprise_connectors::FILE_DOWNLOADED;
    case DeepScanAccessPoint::PRINT:
  }
  NOTREACHED();
}

// LINT.IfChange(AccessPointToUmaHistogramPrefix)
std::string AccessPointToUmaHistogramPrefix(DeepScanAccessPoint access_point) {
  switch (AccessPointToEnterpriseConnector(access_point)) {
    case enterprise_connectors::FILE_TRANSFER:
      return "Enterprise.OnFileTransfer";
    case enterprise_connectors::FILE_ATTACHED:
      return "Enterprise.OnFileAttach";
    case enterprise_connectors::FILE_DOWNLOADED:
      return "Enterprise.OnFileDownload";
    default:
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/histograms.xml:FileUploadEvent)

std::string AccessPointToTriggerString(DeepScanAccessPoint access_point) {
  switch (AccessPointToEnterpriseConnector(access_point)) {
    case enterprise_connectors::FILE_TRANSFER:
      return kFileTransferDataTransferEventTrigger;
    case enterprise_connectors::FILE_ATTACHED:
      return kFileUploadDataTransferEventTrigger;
    case enterprise_connectors::FILE_DOWNLOADED:
      return kFileDownloadDataTransferEventTrigger;
    default:
  }
  NOTREACHED();
}

}  // namespace

FilesRequestHandlerBase::FileInfo::FileInfo() = default;
FilesRequestHandlerBase::FileInfo::FileInfo(FileInfo&& other) = default;
FilesRequestHandlerBase::FileInfo::~FileInfo() = default;

FilesRequestHandlerBase::FilesRequestHandlerBase(
    ContentAnalysisInfoBase* content_analysis_info,
    BinaryUploadService* upload_service,
    GURL url,
    const std::string& content_transfer_method,
    DeepScanAccessPoint access_point,
    std::unique_ptr<FilesRequestHandlerBase::Delegate> delegate)
    : RequestHandlerBase(content_analysis_info,
                         upload_service,
                         url,
                         access_point),
      content_transfer_method_(content_transfer_method),
      delegate_(std::move(delegate)) {
  if (delegate_) {
    delegate_->SetHandler(this);
  }
}

FilesRequestHandlerBase::~FilesRequestHandlerBase() {
  if (!delegate_) {
    return;
  }

  delegate_->MaybeCancelAndReport();
}

void FilesRequestHandlerBase::ReportCanceledFile(size_t index) {
  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis)) {
    return;
  }

  const FileInfo& file_info = delegate_->GetFileInfo(index);
  MaybeReportDeepScanningVerdict(
      delegate_->GetReportingEventRouter(), content_analysis_info_.get(),
      delegate_->GetSource(), delegate_->GetDestination(),
      delegate_->GetPath(index).AsUTF8Unsafe(), file_info.sha256_or_cb,
      file_info.mime_type, AccessPointToTriggerString(access_point_),
      content_transfer_method_,
      content_analysis_info_->GetContentAreaAccountEmail(), file_info.size,
      ScanRequestUploadResult::kUserCancelled,
      enterprise_connectors::ContentAnalysisResponse(), EventResult::CANCELLED);
}

void FilesRequestHandlerBase::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  delegate_->ReportWarningBypass(user_justification, *content_analysis_info_,
                                 AccessPointToTriggerString(access_point_),
                                 content_transfer_method_);
}

base::WeakPtr<FilesRequestHandlerBase> FilesRequestHandlerBase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool FilesRequestHandlerBase::UploadDataImpl() {
  size_t file_count = delegate_->GetFileCount();
  IncrementCrashKey(ScanningCrashKey::PENDING_FILE_UPLOADS, file_count);
  if (file_count != 0) {
    IncrementCrashKey(ScanningCrashKey::TOTAL_FILE_UPLOADS, file_count);
  }
  return delegate_->UploadDataImpl();
}

FileAnalysisRequestBase* FilesRequestHandlerBase::PrepareFileRequest(
    size_t index) {
  auto request = delegate_->CreateFileRequest(
      index, content_analysis_info_->settings(),
      base::BindOnce(&FilesRequestHandlerBase::FileRequestCallback,
                     GetWeakPtr(), index),
      base::BindOnce(&FilesRequestHandlerBase::FileRequestStartCallback,
                     GetWeakPtr(), index));

  FileAnalysisRequestBase* request_raw = request.get();
  content_analysis_info_->InitializeRequest(
      request_raw, /*include_enterprise_only_fields=*/true);
  request_raw->set_analysis_connector(
      AccessPointToEnterpriseConnector(access_point_));
  request_raw->set_source(delegate_->GetSource());
  request_raw->set_destination(delegate_->GetDestination());
  request_raw->GetRequestData(
      base::BindOnce(&FilesRequestHandlerBase::OnGotFileInfo, GetWeakPtr(),
                     std::move(request), index));

  return request_raw;
}

void FilesRequestHandlerBase::OnGotFileInfo(
    std::unique_ptr<BinaryUploadRequest> request,
    size_t index,
    ScanRequestUploadResult result,
    BinaryUploadRequest::Data data) {
  delegate_->UpdateFileInfo(index, data, request.get());

  const auto& analysis_settings = content_analysis_info_->settings();

  // If block large files is enabled, then the file is too large if it exceeds
  // the max upload size limit which is currently 50MB.
  if (result == ScanRequestUploadResult::kSuccess &&
      analysis_settings.block_large_files &&
      data.size > BinaryUploadService::kMaxUploadSizeBytes) {
    result = ScanRequestUploadResult::kFileTooLarge;
  }

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

  UploadFileForDeepScanning(result, delegate_->GetPath(index),
                            std::move(request));
}

void FilesRequestHandlerBase::FinishRequestEarly(
    std::unique_ptr<BinaryUploadRequest> request,
    ScanRequestUploadResult result) {
#if !BUILDFLAG(IS_IOS)
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
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadRequest> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service) {
    upload_service->MaybeUploadForDeepScanning(std::move(request));
  }
}

void FilesRequestHandlerBase::FileRequestCallback(
    size_t index,
    ScanRequestUploadResult upload_result,
    enterprise_connectors::ContentAnalysisResponse response) {
  // Remember to send an ack for this response.  It's possible for the response
  // to be empty and have no request token.  This may happen if Chrome decides
  // to allow the file without uploading with the binary upload service.  For
  // example, zero length files.
  if (upload_result == ScanRequestUploadResult::kSuccess &&
      response.has_request_token()) {
    request_tokens_to_ack_final_actions_[response.request_token()] =
        GetAckFinalAction(response);
  }

  if (upload_result == ScanRequestUploadResult::kTooManyRequests) {
    if (!throttled_) {
      if (auto prefix = AccessPointToUmaHistogramPrefix(access_point_);
          !prefix.empty()) {
        base::UmaHistogramBoolean(prefix + ".Throttled", true);
      }
    }
    throttled_ = true;
  }

  const auto& analysis_settings = content_analysis_info_->settings();
  RequestHandlerResult request_handler_result =
      CalculateRequestHandlerResult(analysis_settings, upload_result, response);
  delegate_->UpdateRequestHandlerResult(index, request_handler_result,
                                        response);
  ++file_result_count_;

  bool result_is_warning = request_handler_result.final_result ==
                           FinalContentAnalysisResult::WARNING;
  const FileInfo& file_info = delegate_->GetFileInfo(index);
  base::TimeTicks start_timestamp = delegate_->GetFileScanStartTime(index);

  if (start_timestamp == base::TimeTicks::Min()) {
    start_timestamp = upload_start_time_;
  }

  RecordDeepScanMetrics(
      analysis_settings.cloud_or_local_settings.is_cloud_analysis(),
      access_point_, base::TimeTicks::Now() - start_timestamp, file_info.size,
      upload_result, response);

  MaybeReportDeepScanningVerdict(
      delegate_->GetReportingEventRouter(), content_analysis_info_.get(),
      delegate_->GetSource(), delegate_->GetDestination(),
      delegate_->GetPath(index).AsUTF8Unsafe(), file_info.sha256_or_cb,
      file_info.mime_type, AccessPointToTriggerString(access_point_),
      content_transfer_method_,
      content_analysis_info_->GetContentAreaAccountEmail(), file_info.size,
      upload_result, response,
      CalculateEventResult(analysis_settings, request_handler_result.complies,
                           result_is_warning));
  delegate_->MarkFileAsReported(index);

  DecrementCrashKey(ScanningCrashKey::PENDING_FILE_UPLOADS);

  delegate_->MaybeCompleteScanRequest();
}

void FilesRequestHandlerBase::FileRequestStartCallback(
    size_t index,
    const BinaryUploadRequest& request) {
  delegate_->SetFileScanStartTime(index);
}

size_t FilesRequestHandlerBase::file_result_count() const {
  return file_result_count_;
}

const std::string& FilesRequestHandlerBase::content_transfer_method() const {
  return content_transfer_method_;
}

}  // namespace enterprise_connectors
