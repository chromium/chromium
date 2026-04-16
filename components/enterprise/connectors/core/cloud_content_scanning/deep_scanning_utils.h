// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_

#include "base/time/time.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"

namespace enterprise_connectors {

class BinaryUploadRequest;

// Helper function to examine a ContentAnalysisResponse and report the
// appropriate events to the enterprise admin. |download_digest_sha256| must be
// encoded using base::HexEncode.  |event_result| indicates whether the user was
// ultimately allowed to access the text or file.
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
    EventResult event_result);

// Helper function to report the user bypassed a warning to the enterprise
// admin. This is split from MaybeReportDeepScanningVerdict since it happens
// after getting a response. |download_digest_sha256| must be encoded using
// base::HexEncode.
void ReportAnalysisConnectorWarningBypass(
    ReportingEventRouter* reporting_event_router,
    const ContentAnalysisInfoBase* content_analysis_info,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const HashCallbackVariant& sha256_or_cb,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    const int64_t content_size,
    const ContentAnalysisResponse& response,
    std::optional<std::u16string> user_justification);

// Returns true for consumer scans and not on enterprise scans.
bool IsConsumerScanRequest(const BinaryUploadRequest& request);

// Returns true if the request will use the scotty resumable upload
// protocol for sending scans to the server.
bool IsResumableUpload(const BinaryUploadRequest& request);

// Returns true if `result` as returned by BinaryUploadService is considered a
// a failed result when attempting a cloud-based multipart content analysis.
bool CloudMultipartResultIsFailure(ScanRequestUploadResult result);

// Returns true if `result` as returned by BinaryUploadService is considered a
// a failed result when attempting a cloud-based resumable content analysis.
bool CloudResumableResultIsFailure(ScanRequestUploadResult result,
                                   bool block_large_files,
                                   bool block_password_protected_files);

// Returns true if `result` as returned by BinaryUploadService is considered a
// a failed result when attempting a local content analysis.
bool LocalResultIsFailure(ScanRequestUploadResult result);

void InitializeBinaryUploadRequest(BinaryUploadRequest* request,
                                   const ContentAnalysisInfoBase& info,
                                   bool include_enterprise_only_fields);

// Calculates the event result that is experienced by the user.
// If data is allowed to be accessed immediately, the result will indicate that
// the user was allowed to use the data independent of the scanning result.
EventResult CalculateEventResult(const AnalysisSettings& settings,
                                 bool allowed_by_scan_result,
                                 bool should_warn);

// Returns true if `result` as returned by BinaryUploadService is considered a
// fail-closed result, regardless of attempting a cloud-based or a local-based
// content analysis.
bool ResultIsFailClosed(ScanRequestUploadResult result);

// Determines if a request result should be used to allow a data use or to
// block it.
bool ResultShouldAllowDataUse(const AnalysisSettings& settings,
                              ScanRequestUploadResult upload_result);

// Calculates the result for the request handler based on the upload result and
// the analysis response.
RequestHandlerResult CalculateRequestHandlerResult(
    const AnalysisSettings& settings,
    ScanRequestUploadResult upload_result,
    const ContentAnalysisResponse& response);

// Helper function to convert a enterprise_connectors::ScanRequestUploadResult
// to a CamelCase string.
std::string BinaryUploadServiceResultToString(
    const ScanRequestUploadResult& result,
    bool success);

// Helper functions to record DeepScanning UMA metrics for the duration of the
// request split by its result and bytes/sec for successful requests.
void RecordDeepScanMetrics(bool is_cloud,
                           DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const ScanRequestUploadResult& result,
                           const ContentAnalysisResponse& response);
void RecordDeepScanMetrics(bool is_cloud,
                           DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success);

// Helper enum and function to manipulate crash keys relevant to scanning.
// If a key would be set to 0, it is unset.
enum class ScanningCrashKey {
  PENDING_FILE_UPLOADS,
  PENDING_TEXT_UPLOADS,
  PENDING_FILE_DOWNLOADS,
  PENDING_PRINTS,
  TOTAL_FILE_UPLOADS,
  TOTAL_TEXT_UPLOADS,
  TOTAL_FILE_DOWNLOADS,
  TOTAL_PRINTS
};
void IncrementCrashKey(ScanningCrashKey key, int delta = 1);
void DecrementCrashKey(ScanningCrashKey key, int delta = 1);

// Maps the request's connector and reason to the corresponding
// DeepScanAccessPoint.
DeepScanAccessPoint AccessPointFromRequest(
    AnalysisConnector connector,
    ContentAnalysisRequest::Reason reason);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
