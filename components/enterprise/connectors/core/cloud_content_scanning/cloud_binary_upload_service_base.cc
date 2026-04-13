// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/cloud_binary_upload_service_base.h"

#include "base/metrics/histogram_functions.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/features.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

const char kSbEnterpriseUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/scan";

const char kSbConsumerUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/consumer";

}  // namespace

CloudBinaryUploadServiceBase::CloudBinaryUploadServiceBase() = default;

CloudBinaryUploadServiceBase::~CloudBinaryUploadServiceBase() = default;

size_t CloudBinaryUploadServiceBase::GetParallelActiveRequestsMax() {
  size_t experiment_max = kParallelContentAnalysisRequestCountMax.Get();
  if (experiment_max > 0) {
    return experiment_max;
  }

  return kDefaultMaxParallelActiveRequests;
}

GURL CloudBinaryUploadServiceBase::GetUploadUrl(
    bool is_consumer_scan_eligible) {
  if (is_consumer_scan_eligible) {
    return GURL(kSbConsumerUploadUrl);
  } else {
    return GURL(kSbEnterpriseUploadUrl);
  }
}

bool CloudBinaryUploadServiceBase::ResponseIsComplete(
    BinaryUploadRequest::Id request_id) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return false;
  }

  for (const std::string& tag : request->content_analysis_request().tags()) {
    if (tag == enterprise_connectors::kMalwareTag &&
        request->should_skip_malware_scan()) {
      // If the content is too large, we don't do a malware scan.
      continue;
    }
    if (received_connector_results_[request_id].count(tag) == 0) {
      return false;
    }
  }

  return true;
}

BinaryUploadRequest* CloudBinaryUploadServiceBase::GetRequest(
    BinaryUploadRequest::Id request_id) {
  auto it = active_requests_.find(request_id);
  if (it != active_requests_.end()) {
    return it->second.get();
  }

  return nullptr;
}

void CloudBinaryUploadServiceBase::RecordRequestMetrics(
    BinaryUploadRequest::Id request_id,
    ScanRequestUploadResult result) {
  base::UmaHistogramEnumeration("SafeBrowsingBinaryUploadRequest.Result",
                                result);

  auto duration = base::TimeTicks::Now() - start_times_[request_id];
  base::UmaHistogramCustomTimes("SafeBrowsingBinaryUploadRequest.Duration",
                                duration, base::Milliseconds(1),
                                base::Minutes(6), 50);

  BinaryUploadRequest* request = GetRequest(request_id);
  if (request && !IsConsumerScanRequest(*request)) {
    std::string request_type;
    switch (request->analysis_connector()) {
      case enterprise_connectors::FILE_DOWNLOADED:
      case enterprise_connectors::FILE_ATTACHED:
      case enterprise_connectors::FILE_TRANSFER:
        request_type = "File";
        break;
      case enterprise_connectors::BULK_DATA_ENTRY:
        request_type = "Text";
        break;
      case enterprise_connectors::PRINT:
        request_type = "Print";
        break;
      case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
        break;
    }
    if (request_type.empty()) {
      return;
    }

    std::string protocol = enterprise_connectors::IsResumableUpload(*request)
                               ? "Resumable"
                               : "Multipart";

    // Example values:
    //   "Enterprise.ResumableRequest.Print.Duration
    //   "Enterprise.MultipartRequest.Text.Duration
    //   "Enterprise.ResumableRequest.File.Result
    base::UmaHistogramCustomTimes(
        base::StrCat(
            {"Enterprise.", protocol, "Request.", request_type, ".Duration"}),
        duration, base::Milliseconds(1), base::Minutes(6), 50);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Enterprise.", protocol, "Request.", request_type, ".Result"}),
        result);
  }
}

void CloudBinaryUploadServiceBase::RecordRequestMetrics(
    BinaryUploadRequest::Id request_id,
    ScanRequestUploadResult result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  RecordRequestMetrics(request_id, result);
  for (const auto& response_result : response.results()) {
    if (response_result.tag() == "malware") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.MalwareResult",
          response_result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
    if (response_result.tag() == "dlp") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.DlpResult",
          response_result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
  }
}

}  // namespace enterprise_connectors
