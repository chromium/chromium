// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"

#include "components/enterprise/connectors/core/common.h"

namespace enterprise_connectors {

namespace {

std::string MaybeGetUnscannedReason(ScanRequestUploadResult result) {
  switch (result) {
    case ScanRequestUploadResult::kSuccess:
    case ScanRequestUploadResult::kUnauthorized:
      // Don't report an unscanned file event on these results.
      return "";

    case ScanRequestUploadResult::kFileTooLarge:
      return kFileTooLargeUnscannedReason;
    case ScanRequestUploadResult::kTooManyRequests:
      return kTooManyRequestsUnscannedReason;
    case ScanRequestUploadResult::kTimeout:
      return kTimeoutUnscannedReason;
    case ScanRequestUploadResult::kUnknown:
    case ScanRequestUploadResult::kUploadFailure:
    case ScanRequestUploadResult::kFailedToGetToken:
    case ScanRequestUploadResult::kIncompleteResponse:
      return kServiceUnavailableUnscannedReason;
    case ScanRequestUploadResult::kFileEncrypted:
      return kFilePasswordProtectedUnscannedReason;
  }
}

}  // namespace

void MaybeReportDeepScanningVerdict(
    ReportingEventRouter* reporting_event_router,
    const ContentAnalysisInfoBase* content_analysis_info,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    const std::string& source_email,
    const int64_t content_size,
    ScanRequestUploadResult result,
    const ContentAnalysisResponse& response,
    EventResult event_result) {
  DCHECK(std::ranges::all_of(download_digest_sha256, base::IsHexDigit<char>));
  DCHECK(content_analysis_info);

  if (!reporting_event_router) {
    return;
  }

  std::string unscanned_reason = MaybeGetUnscannedReason(result);
  if (!unscanned_reason.empty()) {
    reporting_event_router->OnUnscannedFileEvent(
        GURL(content_analysis_info->url()), content_analysis_info->tab_url(),
        source, destination, file_name, download_digest_sha256, mime_type,
        trigger, unscanned_reason, content_transfer_method, content_size,
        event_result);
  }

  if (result != ScanRequestUploadResult::kSuccess) {
    return;
  }

  for (const auto& response_result : response.results()) {
    if (response_result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      unscanned_reason = "UNSCANNED_REASON_UNKNOWN";
      if (response_result.tag() == "malware") {
        unscanned_reason = "MALWARE_SCAN_FAILED";
      } else if (response_result.tag() == "dlp") {
        unscanned_reason = "DLP_SCAN_FAILED";
      }

      reporting_event_router->OnUnscannedFileEvent(
          GURL(content_analysis_info->url()), content_analysis_info->tab_url(),
          source, destination, file_name, download_digest_sha256, mime_type,
          trigger, std::move(unscanned_reason), content_transfer_method,
          content_size, event_result);
    } else if (response_result.triggered_rules_size() > 0) {
      reporting_event_router->OnAnalysisConnectorResult(
          GURL(content_analysis_info->url()), content_analysis_info->tab_url(),
          source, destination, file_name, download_digest_sha256, mime_type,
          trigger, response.request_token(), content_transfer_method,
          source_email, content_analysis_info->GetContentAreaAccountEmail(),
          response_result, content_size,
          content_analysis_info->referrer_chain(),
          content_analysis_info->frame_url_chain(), event_result);
    }
  }
}

}  // namespace enterprise_connectors
