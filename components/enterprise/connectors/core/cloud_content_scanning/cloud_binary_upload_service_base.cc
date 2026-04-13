// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/cloud_binary_upload_service_base.h"

#include "base/metrics/histogram_functions.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/features.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#endif

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
    if (tag == kMalwareTag && request->should_skip_malware_scan()) {
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

void CloudBinaryUploadServiceBase::MaybeRunAuthorizationCallbacks(
    const std::string& dm_token,
    AnalysisConnector connector) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  if (!can_upload_enterprise_data_.contains(token_and_connector)) {
    return;
  }

  // TODO(crbug.com/402435358): Add test coverage to catch this regression
  // after FCM service is completely removed.
  auto it = authorization_callbacks_.find(token_and_connector);
  if (it == authorization_callbacks_.end()) {
    return;
  }
  // To avoid race condition, save the callback and erase it from the map
  // before running it.
  std::unique_ptr<base::OnceCallbackList<void(
      enterprise_connectors::ScanRequestUploadResult)>>
      callbacks = std::move(it->second);
  authorization_callbacks_.erase(it);
  callbacks->Notify(can_upload_enterprise_data_[token_and_connector]);
}

void CloudBinaryUploadServiceBase::LogResponseDebugInfo(
    const std::string& upload_info,
    ScanRequestUploadResult result,
    BinaryUploadRequest* request,
    const ContentAnalysisResponse& response) {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanRequests(request->per_profile_request(),
                              request->access_token(), upload_info,
                              request->GetUrlWithParams().spec(),
                              request->content_analysis_request());
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanResponses(active_tokens_[request->id()],
                               ScanRequestUploadResultToString(result),
                               response);
#endif
}

void CloudBinaryUploadServiceBase::FinishRequest(
    BinaryUploadRequest* request,
    ScanRequestUploadResult result,
    ContentAnalysisResponse response) {
  RecordRequestMetrics(request->id(), result, response);
  std::string upload_info = "None";
  if (!request->IsAuthRequest()) {
    if (const auto it = active_uploads_.find(request->id());
        it != active_uploads_.end()) {
      upload_info = it->second->GetUploadInfo();
    }
  }

  // Always record deep scan request here to ensure it is invoked after http
  // headers are attached.
  LogResponseDebugInfo(upload_info, result, request, response);

  request->FinishRequest(result, response);
}

void CloudBinaryUploadServiceBase::FinishAndCleanupRequest(
    BinaryUploadRequest* request,
    ScanRequestUploadResult result,
    ContentAnalysisResponse response) {
  FinishRequest(request, result, response);
  CleanupRequest(request);
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
      case FILE_DOWNLOADED:
      case FILE_ATTACHED:
      case FILE_TRANSFER:
        request_type = "File";
        break;
      case BULK_DATA_ENTRY:
        request_type = "Text";
        break;
      case PRINT:
        request_type = "Print";
        break;
      case ANALYSIS_CONNECTOR_UNSPECIFIED:
        break;
    }
    if (request_type.empty()) {
      return;
    }

    std::string protocol =
        IsResumableUpload(*request) ? "Resumable" : "Multipart";

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
    const ContentAnalysisResponse& response) {
  RecordRequestMetrics(request_id, result);
  for (const auto& response_result : response.results()) {
    if (response_result.tag() == "malware") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.MalwareResult",
          response_result.status() != ContentAnalysisResponse::Result::FAILURE);
    }
    if (response_result.tag() == "dlp") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.DlpResult",
          response_result.status() != ContentAnalysisResponse::Result::FAILURE);
    }
  }
}

void CloudBinaryUploadServiceBase::PopRequestQueue() {
  AssertCalledOnUIThread();
  while (active_requests_.size() < GetParallelActiveRequestsMax() &&
         !request_queue_.empty()) {
    auto request = std::move(request_queue_.front());
    request_queue_.pop_front();
    UploadForDeepScanning(std::move(request));
  }
}

void CloudBinaryUploadServiceBase::CleanupRequest(
    BinaryUploadRequest* request) {
  AssertCalledOnUIThread();
  BinaryUploadRequest::Id request_id = request->id();
  std::string dm_token = request->device_token();
  auto connector = request->analysis_connector();
  std::string action_id = request->user_action_id();
  active_requests_.erase(request_id);
  active_timers_.erase(request_id);
  active_uploads_.erase(request_id);
  received_connector_results_.erase(request_id);
  active_tokens_.erase(request_id);
  start_times_.erase(request_id);

  MaybeRunAuthorizationCallbacks(dm_token, connector);
  MaybeTrackUploadUserCancellation(action_id);
  // Now that a request has been cleaned up, we can try to allocate resources
  // for queued uploads.
  PopRequestQueue();
}

void CloudBinaryUploadServiceBase::MaybeTrackUploadUserCancellation(
    const std::string& action_id) {
  if (!action_id.empty() && user_action_data_.contains(action_id) &&
      user_action_data_[action_id].cancelled_time.has_value() &&
      CheckForUserActionDone(action_id)) {
    base::TimeDelta total_duration =
        base::TimeTicks::Now() -
        user_action_data_[action_id].cancelled_time.value();
    RecordDeepScanMetrics(user_action_data_[action_id].is_cloud,
                          user_action_data_[action_id].access_point,
                          total_duration, 0, "CancelledByUserCancellationTime",
                          false);

    user_action_data_.erase(action_id);
  }
}

bool CloudBinaryUploadServiceBase::CheckForUserActionDone(
    const std::string& action_id) {
  for (const auto& request : request_queue_) {
    if (request->user_action_id() == action_id) {
      return false;
    }
  }
  for (const auto& it : active_requests_) {
    if (it.second->user_action_id() == action_id) {
      return false;
    }
  }
  return true;
}

void CloudBinaryUploadServiceBase::AssertCalledOnUIThread() {
  DCHECK(ui_task_runner_ && ui_task_runner_->RunsTasksInCurrentSequence());
}

}  // namespace enterprise_connectors
