// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/cloud_binary_upload_service_base.h"

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/multipart_uploader.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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

constexpr base::TimeDelta kAuthTimeout = base::Seconds(10);
constexpr base::TimeDelta kScanningTimeout = base::Minutes(5);

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(bool is_app) {
  if (is_app) {
    return net::DefineNetworkTrafficAnnotation(
        "safe_browsing_binary_upload_app", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "For users opted in to Enhanced Safe Browsing or Google's Advanced "
            "Protection Program, when a file is downloaded, Chrome may upload "
            "that file to Safe Browsing for detailed scanning."
          trigger:
            "The browser will upload the file to Google when the user "
            "downloads a suspicious file and the user is opted in to Enhanced "
            "Safe Browsing or Google's Advanced Protection Program."
          data:
            "The downloaded file and metadata about how the user came to "
            "download that file (including URLs)."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/safe_browsing/cloud_content_scanning/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: FILE_DATA
          }
          last_reviewed: "2023-07-28"
        }
        policy {
          cookies_allowed: NO
          setting: "This is disabled by default an can only be enabled by "
            "opting in to Enhanced Safe Browsing or the Advanced Protection "
            "Program."
          chrome_policy {
            SafeBrowsingDeepScanningEnabled: {
              SafeBrowsingDeepScanningEnabled: false
            }
          }
        }
        )");
  } else {
    return net::DefineNetworkTrafficAnnotation(
        "safe_browsing_binary_upload_connector", R"(
        semantics {
          sender: "Chrome Enterprise Connectors"
          description:
            "For users with content analysis Chrome Enterprise Connectors "
            "enabled, Chrome will upload the data corresponding to the "
            "Connector for scanning."
          trigger:
            "If the OnFileAttachedEnterpriseConnector, "
            "OnFileDownloadedEnterpriseConnector, "
            "OnFileTransferEnterpriseConnector, "
            "OnBulkDataEntryEnterpriseConnector or OnPrintEnterpriseConnector "
            "policy is set, a request is made to scan a file attached to "
            "Chrome, a file downloaded by Chrome, a file transfered from a "
            "ChromeOS file system, data pasted in "
            "Chrome or data printed from Chrome respectively."
          data:
            "The uploaded/downloaded/transfered file, pasted data or printed "
            "data. Also includes an access token (enterprise only)."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/safe_browsing/cloud_content_scanning/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: FILE_DATA
            type: USER_CONTENT
            type: WEB_CONTENT
          }
          last_reviewed: "2023-07-28"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting: "This is disabled by default an can only be enabled by "
            "policy."
          chrome_policy {
            OnFileAttachedEnterpriseConnector {
              OnFileAttachedEnterpriseConnector: "[]"
            }
            OnFileDownloadedEnterpriseConnector {
              OnFileDownloadedEnterpriseConnector: "[]"
            }
            OnBulkDataEntryEnterpriseConnector {
              OnBulkDataEntryEnterpriseConnector: "[]"
            }
            OnFileTransferEnterpriseConnector {
              OnFileTransferEnterpriseConnector: "[]"
            }
            OnPrintEnterpriseConnector {
              OnPrintEnterpriseConnector: "[]"
            }
          }
        }
        )");
  }
}

bool IgnoreErrorResultForResumableUpload(BinaryUploadRequest* request,
                                         ScanRequestUploadResult result) {
  return IsResumableUpload(*request) &&
         (result == ScanRequestUploadResult::kFileTooLarge ||
          result == ScanRequestUploadResult::kFileEncrypted);
}

// Auth request class that is used to check if the browser can upload
// content to the server.
class ValidateDataUploadRequest : public BinaryUploadRequest {
 public:
  ValidateDataUploadRequest(
      ContentAnalysisCallback callback,
      CloudAnalysisSettings settings,
      BrowserPolicyConnectorGetter policy_connector_getter)
      : BinaryUploadRequest(std::move(callback),
                            CloudOrLocalAnalysisSettings(std::move(settings)),
                            std::move(policy_connector_getter)) {}
  ValidateDataUploadRequest(const ValidateDataUploadRequest&) = delete;
  ValidateDataUploadRequest& operator=(const ValidateDataUploadRequest&) =
      delete;
  ~ValidateDataUploadRequest() override = default;

 private:
  // BinaryUploadRequest implementation.
  void GetRequestData(DataCallback callback) override;

  bool IsAuthRequest() const override;
};

inline void ValidateDataUploadRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(
      enterprise_connectors::ScanRequestUploadResult::kSuccess,
      BinaryUploadRequest::Data());
}

bool ValidateDataUploadRequest::IsAuthRequest() const {
  return true;
}

}  // namespace

CloudBinaryUploadServiceBase::CloudBinaryUploadServiceBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      ui_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

CloudBinaryUploadServiceBase::~CloudBinaryUploadServiceBase() = default;



void CloudBinaryUploadServiceBase::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  AssertCalledOnUIThread();

  if (IsConsumerScanRequest(*request)) {
    auto consumer_auth_result = GetConsumerAuthResult(*request);
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       consumer_auth_result);
    return;
  }

  std::optional<ScanRequestUploadResult> enterprise_auth_result =
      MaybeGetEnterpriseAuthResult(*request);
  if (enterprise_auth_result.has_value()) {
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       enterprise_auth_result.value());
    return;
  }

  // Get data from `request` before calling `IsAuthorized` since it is about
  // to move.
  GURL url = request->GetUrlWithParams();
  bool per_profile_request = request->per_profile_request();
  std::string dm_token = request->device_token();
  auto connector = request->analysis_connector();

  // Send a new auth request to compute the result.
  IsAuthorized(
      std::move(url), per_profile_request,
      base::BindOnce(
          &CloudBinaryUploadServiceBase::MaybeUploadForDeepScanningCallback,
          weakptr_factory_.GetWeakPtr(), std::move(request)),
      dm_token, connector);
}

void CloudBinaryUploadServiceBase::MaybeCancelRequests(
    std::unique_ptr<BinaryUploadCancelRequests> cancel) {
  AssertCalledOnUIThread();

  std::string action_id = cancel->get_user_action_id();
  if (user_action_data_.contains(action_id)) {
    user_action_data_[action_id].cancelled_time = base::TimeTicks::Now();
  }

  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis)) {
    return;
  }

  base::EraseIf(
      request_queue_,
      [&cancel](const std::unique_ptr<BinaryUploadRequest>& request) {
        if (request->user_action_id() == cancel->get_user_action_id()) {
          request->FinishRequest(ScanRequestUploadResult::kUserCancelled,
                                 ContentAnalysisResponse());
          return true;
        }
        return false;
      });

  // Also cancel active requests.
  std::vector<BinaryUploadRequest::Id> ids_to_cancel;
  for (const auto& it : active_requests_) {
    if (it.second->user_action_id() == cancel->get_user_action_id()) {
      ids_to_cancel.push_back(it.first);
    }
  }

  for (const auto& id : ids_to_cancel) {
    FinishIfActive(id, ScanRequestUploadResult::kUserCancelled,
                   ContentAnalysisResponse());
  }
}

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
  std::unique_ptr<base::OnceCallbackList<void(ScanRequestUploadResult)>>
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

void CloudBinaryUploadServiceBase::IsAuthorized(const GURL& url,
                                                bool per_profile_request,
                                                AuthorizationCallback callback,
                                                const std::string& dm_token,
                                                AnalysisConnector connector) {
  // Start |timer_| on the first call to IsAuthorized. This is necessary in
  // order to invalidate the authorization every 24 hours.
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, base::Hours(24),
                 base::BindRepeating(
                     &CloudBinaryUploadServiceBase::ResetAuthorizationData,
                     weakptr_factory_.GetWeakPtr(), url));
  }

  TokenAndConnector token_and_connector = {dm_token, connector};
  // Validate if `token_and_connector` is authorized to upload data if this is
  // the first time or the previous check failed.
  if (!can_upload_enterprise_data_.contains(token_and_connector) ||
      can_upload_enterprise_data_[token_and_connector] !=
          ScanRequestUploadResult::kSuccess) {
    // Send a request to check if the browser can upload data.
    auto [iter, inserted] = authorization_callbacks_.try_emplace(
        token_and_connector,
        std::make_unique<
            base::OnceCallbackList<void(ScanRequestUploadResult)>>());
    iter->second->AddUnsafe(std::move(callback));

    if (!pending_validate_data_upload_request_.contains(token_and_connector)) {
      pending_validate_data_upload_request_.insert(token_and_connector);
      CloudAnalysisSettings settings;
      settings.analysis_url = url;
      settings.dm_token = dm_token;
      auto request = std::make_unique<ValidateDataUploadRequest>(
          base::BindOnce(&CloudBinaryUploadServiceBase::
                             ValidateDataUploadRequestConnectorCallback,
                         weakptr_factory_.GetWeakPtr(), dm_token, connector),
          std::move(settings), BrowserPolicyConnectorGetter());
      request->set_device_token(dm_token);
      request->set_analysis_connector(connector);
      request->set_per_profile_request(per_profile_request);

#if BUILDFLAG(IS_CHROMEOS)
      // WebProtect handles requests from ChromeOS Managed Guest Sessions
      // differently, as it cannot rely on the GAIA ID to determine whether or
      // not the user has the BCE license.
      ClientMetadata client_metadata;
      client_metadata.set_is_chrome_os_managed_guest_session(
          IsManagedGuestSession());
      request->set_client_metadata(std::move(client_metadata));
#endif

      QueueForDeepScanning(std::move(request));
    }
    return;
  }
  std::move(callback).Run(can_upload_enterprise_data_[token_and_connector]);
}

void CloudBinaryUploadServiceBase::ResetAuthorizationData(const GURL& url) {
  // Clearing |can_upload_enterprise_data_| will make the next
  // call to IsAuthorized send out a request to validate data uploads.
  auto it = can_upload_enterprise_data_.begin();
  while (it != can_upload_enterprise_data_.end()) {
    std::string dm_token = it->first.first;
    AnalysisConnector connector = it->first.second;
    it = can_upload_enterprise_data_.erase(it);
    IsAuthorized(url, /*per_profile_request*/ false, base::DoNothing(),
                 dm_token, connector);
  }
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

void CloudBinaryUploadServiceBase::OnUploadComplete(
    BinaryUploadRequest::Id request_id,
    bool success,
    int http_status,
    const std::string& response_data) {
  OnGetContentAnalysisResponse(request_id, success, http_status, response_data);
  OnContentUploaded(request_id);
}

void CloudBinaryUploadServiceBase::OnGetAccessToken(
    BinaryUploadRequest::Id request_id,
    const std::string& access_token) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  if (!access_token.empty()) {
    request->set_access_token(access_token);
  }

  request->GetRequestData(
      base::BindOnce(&CloudBinaryUploadServiceBase::OnGetRequestData,
                     weakptr_factory_.GetWeakPtr(), request_id));
}

void CloudBinaryUploadServiceBase::OnGetContentAnalysisResponse(
    BinaryUploadRequest::Id request_id,
    bool success,
    int http_status,
    const std::string& response_data) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  if (http_status == net::HTTP_UNAUTHORIZED) {
    FinishRequest(request, ScanRequestUploadResult::kUnauthorized,
                  ContentAnalysisResponse());
    return;
  }

  if (http_status == net::HTTP_TOO_MANY_REQUESTS) {
    FinishRequest(request, ScanRequestUploadResult::kTooManyRequests,
                  ContentAnalysisResponse());
    return;
  }

  if (!success) {
    FinishRequest(request, ScanRequestUploadResult::kUploadFailure,
                  ContentAnalysisResponse());
    return;
  }

  ContentAnalysisResponse response;
  if (!response.ParseFromString(response_data)) {
    FinishRequest(request, ScanRequestUploadResult::kUploadFailure,
                  ContentAnalysisResponse());
    return;
  }

  // Synchronous scans can return results in the initial response proto, so
  // check for those.
  OnGetResponse(request_id, response);
}

void CloudBinaryUploadServiceBase::OnContentUploaded(
    BinaryUploadRequest::Id request_id) {
  if (BinaryUploadRequest* request = GetRequest(request_id); request) {
    CleanupRequest(request);
  }
}

void CloudBinaryUploadServiceBase::OnGetResponse(
    BinaryUploadRequest::Id request_id,
    ContentAnalysisResponse response) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  for (const auto& result : response.results()) {
    if (result.has_tag() && !result.tag().empty()) {
      DVLOG(1) << "BinaryUploadRequest " << request->request_token()
               << " finished scanning tag <" << result.tag() << ">";
      received_connector_results_[request_id][result.tag()] = result;
    }
  }

  MaybeFinishRequest(request_id);
}

void CloudBinaryUploadServiceBase::MaybeFinishRequest(
    BinaryUploadRequest::Id request_id) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  // It's OK to move here since the map entry is about to be removed.
  ContentAnalysisResponse response;
  response.set_request_token(request->request_token());
  for (auto& tag_and_result : received_connector_results_[request_id]) {
    *response.add_results() = std::move(tag_and_result.second);
  }

  // Set `result` to be INCOMPLETE_RESPONSE, if the request is terminated with
  // incomplete response.
  ScanRequestUploadResult result = ScanRequestUploadResult::kSuccess;
  if (!ResponseIsComplete(request_id)) {
    result = ScanRequestUploadResult::kIncompleteResponse;
  } else if (request->is_content_too_large()) {
    result = ScanRequestUploadResult::kFileTooLarge;
  } else if (request->is_content_encrypted()) {
    result = ScanRequestUploadResult::kFileEncrypted;
  }

  FinishRequest(request, result, std::move(response));
}

std::unique_ptr<ConnectorUploadRequest>
CloudBinaryUploadServiceBase::CreateUploadRequest(
    BinaryUploadRequest* request,
    const BinaryUploadRequest::Id& request_id,
    const GURL& url,
    const std::string& metadata,
    const std::string& histogram_suffix,
    bool force_sync_upload,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    BinaryUploadRequest::Data data,
    ScanRequestUploadResult result,
    ResumableUploadRequestBase::OnceRegisterOnGotHashCallback
        register_on_got_hash_callback) {
  auto callback =
      base::BindOnce(&CloudBinaryUploadServiceBase::OnUploadComplete,
                     weakptr_factory_.GetWeakPtr(), request_id);

  auto verdict_received_callback = base::BindOnce(
      &CloudBinaryUploadServiceBase::OnGetContentAnalysisResponse,
      weakptr_factory_.GetWeakPtr(), request_id);
  auto content_uploaded_callback =
      base::BindOnce(&CloudBinaryUploadServiceBase::OnContentUploaded,
                     weakptr_factory_.GetWeakPtr(), request_id);

  std::unique_ptr<ConnectorUploadRequest> upload_request;
  if (request->IsAuthRequest()) {
    upload_request = safe_browsing::MultipartUploadRequest::CreateStringRequest(
        url_loader_factory_, url, metadata, data.contents, histogram_suffix,
        std::move(traffic_annotation), std::move(callback), ui_task_runner_);
  } else if (!data.contents.empty()) {
    upload_request =
        (IsResumableUpload(*request) &&
         base::FeatureList::IsEnabled(kDlpScanPastedImages))
            ? safe_browsing::ResumableUploadRequest::CreateStringRequest(
                  url_loader_factory_, url, metadata, data.contents,
                  request->image_paste() ? ConnectorUploadRequest::IMAGE
                                         : ConnectorUploadRequest::STRING,
                  histogram_suffix, std::move(traffic_annotation),
                  std::move(verdict_received_callback),
                  std::move(content_uploaded_callback), force_sync_upload,
                  ui_task_runner_)
            : safe_browsing::MultipartUploadRequest::CreateStringRequest(
                  url_loader_factory_, url, metadata, data.contents,
                  histogram_suffix, std::move(traffic_annotation),
                  std::move(callback), ui_task_runner_);
  } else if (!data.path.empty()) {
    upload_request =
        IsResumableUpload(*request)
            ? safe_browsing::ResumableUploadRequest::CreateFileRequest(
                  url_loader_factory_, url, metadata, result, data.path,
                  data.size, data.is_obfuscated, histogram_suffix,
                  std::move(traffic_annotation),
                  std::move(verdict_received_callback),
                  std::move(content_uploaded_callback), force_sync_upload,
                  std::move(register_on_got_hash_callback), ui_task_runner_)
            : safe_browsing::MultipartUploadRequest::CreateFileRequest(
                  url_loader_factory_, url, metadata, data.path, data.size,
                  data.is_obfuscated, histogram_suffix,
                  std::move(traffic_annotation), std::move(callback),
                  ui_task_runner_);

  } else if (data.page.IsValid()) {
    upload_request =
        IsResumableUpload(*request)
            ? safe_browsing::ResumableUploadRequest::CreatePageRequest(
                  url_loader_factory_, url, metadata, result,
                  std::move(data.page), histogram_suffix,
                  std::move(traffic_annotation),
                  std::move(verdict_received_callback),
                  std::move(content_uploaded_callback), force_sync_upload,
                  ui_task_runner_)
            : safe_browsing::MultipartUploadRequest::CreatePageRequest(
                  url_loader_factory_, url, metadata, std::move(data.page),
                  histogram_suffix, std::move(traffic_annotation),
                  std::move(callback), ui_task_runner_);
  } else {
    NOTREACHED();
  }

  return upload_request;
}

void CloudBinaryUploadServiceBase::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<BinaryUploadRequest> request,
    ScanRequestUploadResult auth_check_result) {
  // Ignore the request if the browser cannot upload data.
  if (auth_check_result != ScanRequestUploadResult::kSuccess) {
    request->FinishRequest(auth_check_result, ContentAnalysisResponse());
    return;
  }
  QueueForDeepScanning(std::move(request));
}

void CloudBinaryUploadServiceBase::QueueForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  // Track the start time for the entire user action bundle
  std::string action_id = request->user_action_id();
  if (!action_id.empty() && !user_action_data_.contains(action_id)) {
    user_action_data_[action_id] = {
        request->cloud_or_local_settings().is_cloud_analysis(),
        AccessPointFromRequest(request->analysis_connector(),
                               request->reason())};
  }
  if (active_requests_.size() >= GetParallelActiveRequestsMax()) {
    request_queue_.push_back(std::move(request));
  } else {
    UploadForDeepScanning(std::move(request));
  }
}

void CloudBinaryUploadServiceBase::UploadForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  AssertCalledOnUIThread();

  BinaryUploadRequest* raw_request = request.get();
  BinaryUploadRequest::Id id = request_id_generator_.GenerateNextId();
  request->set_id(id);
  request->StartRequest();
  active_requests_[id] = std::move(request);
  start_times_[id] = base::TimeTicks::Now();

  std::string token = raw_request->SetRandomRequestToken();
  active_tokens_[id] = token;

  PrepareRequestForUpload(id);
}

void CloudBinaryUploadServiceBase::PrepareRequestForUpload(
    BinaryUploadRequest::Id request_id) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  if (request->IsAuthRequest()) {
    request->GetRequestData(
        base::BindOnce(&CloudBinaryUploadServiceBase::OnGetRequestData,
                       weakptr_factory_.GetWeakPtr(), request_id));
  } else if (!IsConsumerScanRequest(*request)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetLocalIpAddresses),
        base::BindOnce(&CloudBinaryUploadServiceBase::OnIpAddressesFetched,
                       weakptr_factory_.GetWeakPtr(), request_id));
  } else {
    MaybeGetAccessToken(
        request, base::BindOnce(&CloudBinaryUploadServiceBase::OnGetAccessToken,
                                weakptr_factory_.GetWeakPtr(), request_id));
  }

  // `request` might have been destroyed by `OnGetRequestData`
  request = GetRequest(request_id);
  if (!request) {
    return;
  }

  active_timers_[request_id] = std::make_unique<base::OneShotTimer>();
  active_timers_[request_id]->Start(
      FROM_HERE, request->IsAuthRequest() ? kAuthTimeout : kScanningTimeout,
      base::BindOnce(&CloudBinaryUploadServiceBase::FinishIfActive,
                     weakptr_factory_.GetWeakPtr(), request_id,
                     ScanRequestUploadResult::kTimeout,
                     ContentAnalysisResponse()));
}

void CloudBinaryUploadServiceBase::OnIpAddressesFetched(
    BinaryUploadRequest::Id request_id,
    std::vector<std::string> ip_addresses) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  for (const auto& ip_address : ip_addresses) {
    request->add_local_ips(ip_address);
  }

  MaybeGetAccessToken(
      request, base::BindOnce(&CloudBinaryUploadServiceBase::OnGetAccessToken,
                              weakptr_factory_.GetWeakPtr(), request_id));
}

void CloudBinaryUploadServiceBase::RegisterOnGotHashCallback(
    BinaryUploadRequest::Id request_id,
    OnGotHashCallback on_got_hash_callback) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    std::move(on_got_hash_callback).Run("");
    return;
  }
  // If the request's hash has completed, the parameter callback will run
  // immediately. Post it as a task so it runs after this function returns. Also
  // set call_last to true since the parameter callback can delete the request.
  request->register_on_got_hash_callback_.Run(
      /*call_last=*/true,
      base::BindPostTaskToCurrentDefault(std::move(on_got_hash_callback)));
}

void CloudBinaryUploadServiceBase::FinishIfActive(
    BinaryUploadRequest::Id request_id,
    ScanRequestUploadResult result,
    ContentAnalysisResponse response) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (request) {
    FinishAndCleanupRequest(request, result, response);
  }
}

void CloudBinaryUploadServiceBase::OnGetRequestData(
    BinaryUploadRequest::Id request_id,
    ScanRequestUploadResult get_data_result,
    BinaryUploadRequest::Data data) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request ||
      ShouldTerminateRequestEarly(request, get_data_result, data.size)) {
    return;
  }

  // If the file is encrypted, let the service know that the file is
  // encrypted.
  if (get_data_result == ScanRequestUploadResult::kFileEncrypted) {
    request->set_is_content_encrypted(true);
  }
  if (get_data_result == ScanRequestUploadResult::kFileTooLarge) {
    request->set_is_content_too_large(true);
  }
  request->set_should_skip_malware_scan(
      data.size > BinaryUploadService::kMaxUploadSizeBytes);

  ResumableUploadRequestBase::OnceRegisterOnGotHashCallback
      register_on_got_hash_callback = base::NullCallback();
  if (request->digest().empty() && request->register_on_got_hash_callback_) {
    // The hash is being computed. Let the server know the hash will be
    // uploaded as a header during the finalize call.
    request->set_content_hash_in_final_call(true);
    register_on_got_hash_callback =
        base::BindOnce(&CloudBinaryUploadServiceBase::RegisterOnGotHashCallback,
                       weakptr_factory_.GetWeakPtr(), request_id);
  }

  std::string metadata;
  request->SerializeToString(&metadata);
  metadata = base::Base64Encode(metadata);

  GURL url = request->GetUrlWithParams();
  if (!url.is_valid()) {
    url = GetUploadUrl(IsConsumerScanRequest(*request));
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetTrafficAnnotationTag(IsConsumerScanRequest(*request));
  std::string histogram_suffix =
      IsConsumerScanRequest(*request) ? "ConsumerUpload" : "EnterpriseUpload";

  // The downloaded file will not be available for deep scan upload due to the
  // newly introduced download obfuscation step. We must wait for deobfuscation
  // to complete before uploading, which is guaranteed under the pre-async
  // upload behaviour.
  bool force_sync_upload = request->analysis_connector() == FILE_DOWNLOADED;

  std::unique_ptr<ConnectorUploadRequest> upload_request = CreateUploadRequest(
      request, request_id, url, metadata, histogram_suffix, force_sync_upload,
      traffic_annotation, data, get_data_result,
      std::move(register_on_got_hash_callback));
  upload_request->set_access_token(request->access_token());
  upload_request->set_request_token(
      request->content_analysis_request().request_token());

  // |request| might have been deleted by the call to Start() in tests, so don't
  // dereference it afterwards.
  upload_request->Start();
  active_uploads_[request_id] = std::move(upload_request);
}

bool CloudBinaryUploadServiceBase::ShouldTerminateRequestEarly(
    BinaryUploadRequest* request,
    ScanRequestUploadResult get_data_result,
    size_t data_size) {
  if (get_data_result != ScanRequestUploadResult::kSuccess &&
      !IgnoreErrorResultForResumableUpload(request, get_data_result)) {
    FinishAndCleanupRequest(request, get_data_result,
                            ContentAnalysisResponse());
    return true;
  }

  if (!request->IsAuthRequest() && data_size == 0) {
    // A size of 0 implies an edge case like an empty file being uploaded. In
    // such a case, the file doesn't need to scan so the request can simply
    // finish early.
    FinishAndCleanupRequest(request, ScanRequestUploadResult::kSuccess,
                            ContentAnalysisResponse());
    return true;
  }

  return false;
}

std::optional<ScanRequestUploadResult>
CloudBinaryUploadServiceBase::MaybeGetEnterpriseAuthResult(
    const BinaryUploadRequest& request) {
  auto connector = request.analysis_connector();
  std::string dm_token = request.device_token();
  TokenAndConnector token_and_connector = {dm_token, connector};

  if (dm_token.empty()) {
    return ScanRequestUploadResult::kUnauthorized;
  }

  if (!can_upload_enterprise_data_.contains(token_and_connector) ||
      can_upload_enterprise_data_[token_and_connector] !=
          ScanRequestUploadResult::kSuccess) {
    return std::nullopt;
  }

  return can_upload_enterprise_data_[token_and_connector];
}

ScanRequestUploadResult CloudBinaryUploadServiceBase::GetConsumerAuthResult(
    const BinaryUploadRequest& request) {
  DCHECK(!request.IsAuthRequest());

  return IsAdvancedProtection() || IsEnhancedProtection()
             ? ScanRequestUploadResult::kSuccess
             : ScanRequestUploadResult::kUnauthorized;
}

void CloudBinaryUploadServiceBase::ValidateDataUploadRequestConnectorCallback(
    const std::string& dm_token,
    AnalysisConnector connector,
    ScanRequestUploadResult result,
    ContentAnalysisResponse response) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  pending_validate_data_upload_request_.erase(token_and_connector);
  can_upload_enterprise_data_[token_and_connector] = result;
}

}  // namespace enterprise_connectors
