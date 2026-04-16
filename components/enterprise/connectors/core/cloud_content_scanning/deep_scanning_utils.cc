// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/safe_browsing/core/common/features.h"

namespace enterprise_connectors {

namespace {

constexpr int kMinBytesPerSecond = 1;
constexpr int kMaxBytesPerSecond = 100 * 1024 * 1024;  // 100 MB/s

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
    case ScanRequestUploadResult::kFileEncrypted:
      return kFilePasswordProtectedUnscannedReason;
    case ScanRequestUploadResult::kUnknown:
    case ScanRequestUploadResult::kUploadFailure:
    case ScanRequestUploadResult::kFailedToGetToken:
    case ScanRequestUploadResult::kIncompleteResponse:
      return kServiceUnavailableUnscannedReason;
    case ScanRequestUploadResult::kUserCancelled:
      return kUserCancelledUnscannedReason;
  }
}

bool ShouldAllowDeepScanOnLargeOrEncryptedFiles(
    ScanRequestUploadResult result,
    bool block_large_files,
    bool block_password_protected_files) {
  return (result == ScanRequestUploadResult::kFileTooLarge &&
          !block_large_files) ||
         (result == ScanRequestUploadResult::kFileEncrypted &&
          !block_password_protected_files);
}

bool ContentAnalysisActionAllowsDataUse(TriggeredRule::Action action) {
  switch (action) {
    case TriggeredRule::ACTION_UNSPECIFIED:
    case TriggeredRule::REPORT_ONLY:
      return true;
    case TriggeredRule::WARN:
    case TriggeredRule::BLOCK:
    case TriggeredRule::FORCE_SAVE_TO_CLOUD:
      return false;
  }
}

crash_reporter::CrashKeyString<7>* GetScanCrashKey(ScanningCrashKey key) {
  static crash_reporter::CrashKeyString<7> pending_file_uploads(
      "pending-file-upload-scans");
  static crash_reporter::CrashKeyString<7> pending_text_uploads(
      "pending-text-upload-scans");
  static crash_reporter::CrashKeyString<7> pending_file_downloads(
      "pending-file-download-scans");
  static crash_reporter::CrashKeyString<7> pending_prints(
      "pending-print-scans");
  static crash_reporter::CrashKeyString<7> total_file_uploads(
      "total-file-upload-scans");
  static crash_reporter::CrashKeyString<7> total_text_uploads(
      "total-text-upload-scans");
  static crash_reporter::CrashKeyString<7> total_file_downloads(
      "total-file-download-scans");
  static crash_reporter::CrashKeyString<7> total_prints("total-print-scans");
  switch (key) {
    case ScanningCrashKey::PENDING_FILE_UPLOADS:
      return &pending_file_uploads;
    case ScanningCrashKey::PENDING_TEXT_UPLOADS:
      return &pending_text_uploads;
    case ScanningCrashKey::PENDING_FILE_DOWNLOADS:
      return &pending_file_downloads;
    case ScanningCrashKey::PENDING_PRINTS:
      return &pending_prints;
    case ScanningCrashKey::TOTAL_FILE_UPLOADS:
      return &total_file_uploads;
    case ScanningCrashKey::TOTAL_TEXT_UPLOADS:
      return &total_text_uploads;
    case ScanningCrashKey::TOTAL_FILE_DOWNLOADS:
      return &total_file_downloads;
    case ScanningCrashKey::TOTAL_PRINTS:
      return &total_prints;
  }
}

int* GetScanCrashKeyCount(ScanningCrashKey key) {
  static int pending_file_uploads = 0;
  static int pending_text_uploads = 0;
  static int pending_file_downloads = 0;
  static int pending_prints = 0;
  static int total_file_uploads = 0;
  static int total_text_uploads = 0;
  static int total_file_downloads = 0;
  static int total_prints = 0;
  switch (key) {
    case ScanningCrashKey::PENDING_FILE_UPLOADS:
      return &pending_file_uploads;
    case ScanningCrashKey::PENDING_TEXT_UPLOADS:
      return &pending_text_uploads;
    case ScanningCrashKey::PENDING_FILE_DOWNLOADS:
      return &pending_file_downloads;
    case ScanningCrashKey::PENDING_PRINTS:
      return &pending_prints;
    case ScanningCrashKey::TOTAL_FILE_UPLOADS:
      return &total_file_uploads;
    case ScanningCrashKey::TOTAL_TEXT_UPLOADS:
      return &total_text_uploads;
    case ScanningCrashKey::TOTAL_FILE_DOWNLOADS:
      return &total_file_downloads;
    case ScanningCrashKey::TOTAL_PRINTS:
      return &total_prints;
  }
}

void ModifyKey(ScanningCrashKey key, int delta) {
  int* key_value = GetScanCrashKeyCount(key);

  // Since the crash key string length is determined at compile time, ensure the
  // given number is restricted to 6 digits (char 7 is for null terminating the
  // string).
  int new_value = (*key_value) + delta;
  new_value = std::max(0, new_value);
  new_value = std::min(999999, new_value);

  *key_value = new_value;
  crash_reporter::CrashKeyString<7>* crash_key = GetScanCrashKey(key);
  DCHECK(crash_key);

  if (new_value == 0) {
    crash_key->Clear();
  } else {
    crash_key->Set(base::NumberToString(new_value));
  }
}

}  // namespace

void MaybeReportDeepScanningVerdict(
    ReportingEventRouter* reporting_event_router,
    const ContentAnalysisInfoBase* content_analysis_info,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const HashCallbackVariant& sha256_or_cb,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    const std::string& source_email,
    const int64_t content_size,
    ScanRequestUploadResult result,
    const ContentAnalysisResponse& response,
    EventResult event_result) {
  DCHECK(content_analysis_info);

  if (!reporting_event_router) {
    return;
  }

  std::string unscanned_reason = MaybeGetUnscannedReason(result);
  if (!unscanned_reason.empty()) {
    reporting_event_router->OnUnscannedFileEvent(
        GURL(content_analysis_info->url()), content_analysis_info->tab_url(),
        source, destination, file_name, sha256_or_cb, mime_type, trigger,
        response.request_token(), unscanned_reason, content_transfer_method,
        content_size, event_result);
  }

  if (result != ScanRequestUploadResult::kSuccess) {
    return;
  }

  for (const auto& response_result : response.results()) {
    if (response_result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      unscanned_reason = "UNSCANNED_REASON_UNKNOWN";
      if (response_result.tag() == kMalwareTag) {
        unscanned_reason = "MALWARE_SCAN_FAILED";
      } else if (response_result.tag() == kDlpTag) {
        unscanned_reason = "DLP_SCAN_FAILED";
      }

      reporting_event_router->OnUnscannedFileEvent(
          GURL(content_analysis_info->url()), content_analysis_info->tab_url(),
          source, destination, file_name, sha256_or_cb, mime_type, trigger,
          response.request_token(), std::move(unscanned_reason),
          content_transfer_method, content_size, event_result);
    } else if (response_result.triggered_rules_size() > 0) {
      reporting_event_router->OnAnalysisConnectorResult(
          GURL(content_analysis_info->url()), content_analysis_info->tab_url(),
          source, destination, file_name, sha256_or_cb, mime_type, trigger,
          response.request_token(), content_transfer_method, source_email,
          content_analysis_info->GetContentAreaAccountEmail(), response_result,
          content_size, content_analysis_info->referrer_chain(),
          content_analysis_info->frame_url_chain(), event_result);
    }
  }
}

void ReportAnalysisConnectorWarningBypass(
    ReportingEventRouter* reporting_event_router,
    const ContentAnalysisInfoBase* content_analysis_info,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const enterprise_connectors::HashCallbackVariant& sha256_or_cb,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    const int64_t content_size,
    const ContentAnalysisResponse& response,
    std::optional<std::u16string> user_justification) {
  DCHECK(content_analysis_info);

  if (!reporting_event_router) {
    return;
  }

  for (const auto& result : response.results()) {
    // Only report results with triggered rules.
    if (result.triggered_rules().empty()) {
      continue;
    }

    reporting_event_router->OnSensitiveDataEvent(
        GURL(content_analysis_info->url()), content_analysis_info->tab_url(),
        source, destination, file_name, sha256_or_cb, mime_type, trigger,
        response.request_token(), content_transfer_method,
        /*source_email=*/"",
        content_analysis_info->GetContentAreaAccountEmail(), user_justification,
        result, content_size, content_analysis_info->referrer_chain(),
        content_analysis_info->frame_url_chain(), EventResult::BYPASSED);
  }
}

bool IsConsumerScanRequest(const BinaryUploadRequest& request) {
  if (request.cloud_or_local_settings().is_local_analysis()) {
    return false;
  }

  for (const std::string& tag : request.content_analysis_request().tags()) {
    if (tag == kDlpTag) {
      return false;
    }
  }
  return request.device_token().empty();
}

bool IsResumableUpload(const BinaryUploadRequest& request) {
  if (IsConsumerScanRequest(request) ||
      !request.cloud_or_local_settings().is_cloud_analysis()) {
    return false;
  }
  // Use the Resumable request protocol only for image pastes and
  // non-paste requests.
  return request.content_analysis_request().analysis_connector() !=
             AnalysisConnector::BULK_DATA_ENTRY ||
         request.image_paste();
}

bool CloudMultipartResultIsFailure(ScanRequestUploadResult result) {
  return result != ScanRequestUploadResult::kSuccess;
}

bool CloudResumableResultIsFailure(ScanRequestUploadResult result,
                                   bool block_large_files,
                                   bool block_password_protected_files) {
  return result != ScanRequestUploadResult::kSuccess &&
         !ShouldAllowDeepScanOnLargeOrEncryptedFiles(
             result, block_large_files, block_password_protected_files);
}

bool LocalResultIsFailure(ScanRequestUploadResult result) {
  return result != ScanRequestUploadResult::kSuccess &&
         result != ScanRequestUploadResult::kFileTooLarge &&
         result != ScanRequestUploadResult::kFileEncrypted;
}

void InitializeBinaryUploadRequest(BinaryUploadRequest* request,
                                   const ContentAnalysisInfoBase& info,
                                   bool include_enterprise_only_fields) {
  const auto& settings = info.settings();

  if (include_enterprise_only_fields) {
    if (settings.cloud_or_local_settings.is_cloud_analysis()) {
      request->set_device_token(settings.cloud_or_local_settings.dm_token());
    }

    // Include tab page title in local content analysis requests.
    if (settings.cloud_or_local_settings.is_local_analysis()) {
      request->set_tab_title(info.tab_title());
    }

    if (settings.client_metadata) {
      request->set_client_metadata(*settings.client_metadata);
    }

    request->set_per_profile_request(settings.per_profile);

    if (info.reason() != ContentAnalysisRequest::UNKNOWN) {
      request->set_reason(info.reason());
    }

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      request->set_referrer_chain(info.referrer_chain());
    }

    std::string email = info.GetContentAreaAccountEmail();
    if (!email.empty()) {
      request->set_content_area_account_email(email);
    }

    if (base::FeatureList::IsEnabled(kEnterpriseIframeDlpRulesSupport)) {
      request->set_frame_url_chain(info.frame_url_chain());
    }
  }

  request->set_user_action_requests_count(info.user_action_requests_count());
  request->set_user_action_id(info.user_action_id());
  request->set_email(info.email());
  request->set_url(info.url());
  request->set_tab_url(info.tab_url());

  for (const auto& tag : settings.tags) {
    request->add_tag(tag.first);
  }

  request->set_blocking(settings.block_until_verdict !=
                        BlockUntilVerdict::kNoBlock);
}

EventResult CalculateEventResult(const AnalysisSettings& settings,
                                 bool allowed_by_scan_result,
                                 bool should_warn) {
  bool wait_for_verdict =
      settings.block_until_verdict == BlockUntilVerdict::kBlock;
  return (allowed_by_scan_result || !wait_for_verdict)
             ? EventResult::ALLOWED
             : (should_warn ? EventResult::WARNED : EventResult::BLOCKED);
}

bool ResultIsFailClosed(ScanRequestUploadResult result) {
  switch (result) {
    case ScanRequestUploadResult::kUploadFailure:
    case ScanRequestUploadResult::kTimeout:
    case ScanRequestUploadResult::kFailedToGetToken:
    case ScanRequestUploadResult::kTooManyRequests:
    case ScanRequestUploadResult::kUnknown:
    case ScanRequestUploadResult::kIncompleteResponse:
    case ScanRequestUploadResult::kUserCancelled:
      return true;
    case ScanRequestUploadResult::kSuccess:
    case ScanRequestUploadResult::kFileTooLarge:
    case ScanRequestUploadResult::kUnauthorized:
    case ScanRequestUploadResult::kFileEncrypted:
      return false;
  }
}

bool ResultShouldAllowDataUse(const AnalysisSettings& settings,
                              ScanRequestUploadResult upload_result) {
  bool default_action_allow_data_use =
      settings.default_action == DefaultAction::kAllow;

  // Keep this implemented as a switch instead of a simpler if statement so that
  // new values added to ScanRequestUploadResult cause a compiler error.
  switch (upload_result) {
    case ScanRequestUploadResult::kSuccess:
    // UNAUTHORIZED allows data usage since it's a result only obtained if the
    // browser is not authorized to perform deep scanning. It does not make
    // sense to block data in this situation since no actual scanning of the
    // data was performed, so it's allowed.
    case ScanRequestUploadResult::kUnauthorized:
      return true;

    case ScanRequestUploadResult::kUploadFailure:
    case ScanRequestUploadResult::kTimeout:
    case ScanRequestUploadResult::kFailedToGetToken:
    case ScanRequestUploadResult::kTooManyRequests:
    case ScanRequestUploadResult::kUnknown:
    case ScanRequestUploadResult::kIncompleteResponse:
      DVLOG(1) << __func__
               << ": handled by fail-closed settings, "
                  "default_action_allow_data_use="
               << default_action_allow_data_use;
      return default_action_allow_data_use;

    case ScanRequestUploadResult::kFileTooLarge:
      return !settings.block_large_files;

    case ScanRequestUploadResult::kFileEncrypted:
      return !settings.block_password_protected_files;
    case ScanRequestUploadResult::kUserCancelled:
      return false;
  }
}

RequestHandlerResult CalculateRequestHandlerResult(
    const AnalysisSettings& settings,
    ScanRequestUploadResult upload_result,
    const ContentAnalysisResponse& response) {
  std::string tag;
  auto action = GetHighestPrecedenceAction(response, &tag);

  bool file_complies = ResultShouldAllowDataUse(settings, upload_result) &&
                       ContentAnalysisActionAllowsDataUse(action);

  RequestHandlerResult result;
  result.complies = file_complies;
  result.request_token = response.request_token();
  result.tag = tag;

  if (file_complies) {
    result.final_result = FinalContentAnalysisResult::SUCCESS;
    return result;
  }

  // If file is non-compliant, map it to the specific case.
  //
  // We should check if the action is `WARN` or `BLOCK` before `FILE_TOO_LARGE`
  // or `FILE_ENCRYPTED`, because the server could issue a `WARN` or `BLOCK`
  // verdict based on the metadata of large or encrypted files.
  if (ResultIsFailClosed(upload_result)) {
    DVLOG(1) << __func__ << ": result mapped to fail-closed.";
    result.final_result = FinalContentAnalysisResult::FAIL_CLOSED;
  } else if (action == TriggeredRule::WARN) {
    result.final_result = FinalContentAnalysisResult::WARNING;
  } else if (action == TriggeredRule::BLOCK) {
    result.final_result = FinalContentAnalysisResult::FAILURE;
  } else if (upload_result == ScanRequestUploadResult::kFileTooLarge) {
    result.final_result = FinalContentAnalysisResult::LARGE_FILES;
  } else if (upload_result == ScanRequestUploadResult::kFileEncrypted) {
    result.final_result = FinalContentAnalysisResult::ENCRYPTED_FILES;
  } else {
    result.final_result = FinalContentAnalysisResult::FAILURE;
  }

  for (const auto& response_result : response.results()) {
    if (!response_result.has_status() ||
        response_result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }
    for (const auto& rule : response_result.triggered_rules()) {
      // Ensures that lower precedence actions custom messages are skipped. The
      // message shown is arbitrary for rules with the same precedence.
      if (rule.action() == action && rule.has_custom_rule_message()) {
        result.custom_rule_message = rule.custom_rule_message();
      }
    }
  }
  return result;
}

void RecordDeepScanMetrics(bool is_cloud,
                           DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const ScanRequestUploadResult& result,
                           const ContentAnalysisResponse& response) {
  // Don't record UMA metrics for this result.
  if (result == ScanRequestUploadResult::kUnauthorized) {
    return;
  }
  bool dlp_verdict_success = true;
  bool malware_verdict_success = true;
  for (const auto& response_result : response.results()) {
    if (response_result.tag() == "dlp" &&
        response_result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      dlp_verdict_success = false;
    }
    if (response_result.tag() == "malware" &&
        response_result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      malware_verdict_success = false;
    }
  }

  bool success = dlp_verdict_success && malware_verdict_success;
  std::string result_value = BinaryUploadServiceResultToString(result, success);

  // Update |success| so non-SUCCESS results don't log the bytes/sec metric.
  success &= (result == ScanRequestUploadResult::kSuccess);

  RecordDeepScanMetrics(is_cloud, access_point, duration, total_bytes,
                        result_value, success);
}

void RecordDeepScanMetrics(bool is_cloud,
                           DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success) {
  // Don't record metrics if the duration is unusable.
  if (duration.InMilliseconds() == 0) {
    return;
  }

  const char* prefix =
      is_cloud ? "SafeBrowsing.DeepScan." : "SafeBrowsing.LocalDeepScan.";

  std::string access_point_string = DeepScanAccessPointToString(access_point);
  if (success) {
    base::UmaHistogramCustomCounts(
        prefix + access_point_string + ".BytesPerSeconds",
        (1000 * total_bytes) / duration.InMilliseconds(),
        /*min=*/kMinBytesPerSecond,
        /*max=*/kMaxBytesPerSecond,
        /*buckets=*/50);
  }

  // The scanning timeout is 5 minutes, so the bucket maximum time is 30 minutes
  // in order to be lenient and avoid having lots of data in the overflow
  // bucket.
  base::UmaHistogramCustomTimes(
      prefix + access_point_string + "." + result + ".Duration", duration,
      base::Milliseconds(1), base::Minutes(30), 50);
  base::UmaHistogramCustomTimes(prefix + access_point_string + ".Duration",
                                duration, base::Milliseconds(1),
                                base::Minutes(30), 50);
}

std::string BinaryUploadServiceResultToString(
    const ScanRequestUploadResult& result,
    bool success) {
  switch (result) {
    case ScanRequestUploadResult::kSuccess:
      if (success) {
        return "Success";
      } else {
        return "FailedToGetVerdict";
      }
    case ScanRequestUploadResult::kUploadFailure:
      return "UploadFailure";
    case ScanRequestUploadResult::kTimeout:
      return "Timeout";
    case ScanRequestUploadResult::kFileTooLarge:
      return "FileTooLarge";
    case ScanRequestUploadResult::kFailedToGetToken:
      return "FailedToGetToken";
    case ScanRequestUploadResult::kUnknown:
      return "Unknown";
    case ScanRequestUploadResult::kUnauthorized:
      return "";
    case ScanRequestUploadResult::kFileEncrypted:
      return "FileEncrypted";
    case ScanRequestUploadResult::kTooManyRequests:
      return "TooManyRequests";
    case ScanRequestUploadResult::kIncompleteResponse:
      return "IncompleteResponse";
    case ScanRequestUploadResult::kUserCancelled:
      return "UserCancelled";
  }
}

void IncrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, delta);
}

void DecrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, -delta);
}

DeepScanAccessPoint AccessPointFromRequest(
    AnalysisConnector connector,
    ContentAnalysisRequest::Reason reason) {
  switch (connector) {
    case FILE_DOWNLOADED:
      return DeepScanAccessPoint::DOWNLOAD;
    case FILE_ATTACHED:
      if (reason == ContentAnalysisRequest::DRAG_AND_DROP) {
        return DeepScanAccessPoint::DRAG_AND_DROP;
      }
      if (reason == ContentAnalysisRequest::CLIPBOARD_PASTE) {
        return DeepScanAccessPoint::PASTE;
      }
      return DeepScanAccessPoint::UPLOAD;
    case BULK_DATA_ENTRY:
      return DeepScanAccessPoint::PASTE;
    case PRINT:
      return DeepScanAccessPoint::PRINT;
    case FILE_TRANSFER:
      return DeepScanAccessPoint::FILE_TRANSFER;
    case ANALYSIS_CONNECTOR_UNSPECIFIED:
      return DeepScanAccessPoint::UPLOAD;
  }
}

}  // namespace enterprise_connectors
