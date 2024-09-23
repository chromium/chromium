// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

const size_t kNumFailuresToEnforceBackoff = 3;
const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.

const size_t kLookupTimeoutDurationInSeconds = 3;

void LogThreatInfoSize(int num_full_hash_matches, bool is_source_local_cache) {
  base::UmaHistogramCounts100("SafeBrowsing.HPRT.ThreatInfoSize",
                              num_full_hash_matches);
  std::string breakout_histogram =
      is_source_local_cache ? "SafeBrowsing.HPRT.ThreatInfoSize.LocalCache"
                            : "SafeBrowsing.HPRT.ThreatInfoSize.NetworkRequest";
  base::UmaHistogramCounts100(breakout_histogram, num_full_hash_matches);
}

SBThreatType MapFullHashDetailToSbThreatType(
    const V5::FullHash::FullHashDetail& detail) {
  // Note that for hash-prefix real-time checks, there is no need to use
  // the FRAME_ONLY enum in the attributes field, because all the checks are for
  // frame URLs.
  if (base::Contains(detail.attributes(), V5::ThreatAttribute::CANARY)) {
    return SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE;
  }
  switch (detail.threat_type()) {
    case V5::ThreatType::MALWARE:
      return SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
    case V5::ThreatType::SOCIAL_ENGINEERING:
      return SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    case V5::ThreatType::UNWANTED_SOFTWARE:
      return SBThreatType::SB_THREAT_TYPE_URL_UNWANTED;
    case V5::ThreatType::TRICK_TO_BILL:
      return SBThreatType::SB_THREAT_TYPE_BILLING;
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      NOTREACHED_IN_MIGRATION()
          << "Unexpected ThreatType encountered: " << detail.threat_type();
      return SBThreatType::SB_THREAT_TYPE_UNUSED;
  }
}

// The OHTTP client that accepts OnCompleted calls and forwards them to the
// provided callback. It also calls the callback with empty response and
// net::ERR_FAILED error if it is destroyed before the callback is called.
class ObliviousHttpClient : public network::mojom::ObliviousHttpClient {
 public:
  using OnCompletedCallback =
      base::OnceCallback<void(const std::optional<std::string>&,
                              int,
                              int,
                              scoped_refptr<net::HttpResponseHeaders>,
                              bool)>;

  explicit ObliviousHttpClient(OnCompletedCallback callback)
      : callback_(std::move(callback)) {}

  ~ObliviousHttpClient() override {
    if (!called_) {
      std::move(callback_).Run(std::nullopt, net::ERR_FAILED,
                               /*response_code=*/0, /*headers=*/nullptr,
                               /*ohttp_client_destructed_early=*/true);
    }
  }

  void OnCompleted(
      network::mojom::ObliviousHttpCompletionResultPtr status) override {
    if (called_) {
      mojo::ReportBadMessage("OnCompleted called more than once");
      return;
    }
    called_ = true;

    std::optional<std::string> response_body;
    int net_error;
    int response_code;
    scoped_refptr<net::HttpResponseHeaders> response_headers;
    std::string histogram_suffix;
    if (status->is_net_error()) {
      net_error = status->get_net_error();
      response_code = 0;
      histogram_suffix = "NetErrorResult";
    } else if (status->is_outer_response_error_code()) {
      net_error = net::ERR_HTTP_RESPONSE_CODE_FAILURE;
      response_code = status->get_outer_response_error_code();
      histogram_suffix = "OuterResponseResult";
    } else {
      DCHECK(status->is_inner_response());
      response_headers = std::move(status->get_inner_response()->headers);
      histogram_suffix = "InnerResponseResult";
      if (status->get_inner_response()->response_code != net::HTTP_OK) {
        net_error = net::ERR_HTTP_RESPONSE_CODE_FAILURE;
        response_code = status->get_inner_response()->response_code;
      } else {
        response_body = status->get_inner_response()->response_body;
        net_error = net::OK;
        response_code = net::HTTP_OK;
      }
    }
    RecordHttpResponseOrErrorCode(
        ("SafeBrowsing.HPRT.Network." + histogram_suffix).c_str(), net_error,
        response_code);
    std::move(callback_).Run(response_body, net_error, response_code,
                             response_headers,
                             /*ohttp_client_destructed_early=*/false);
  }

 private:
  bool called_ = false;
  OnCompletedCallback callback_;
};

}  // namespace

HashRealTimeService::HashRealTimeService(
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        get_network_context,
    VerdictCacheManager* cache_manager,
    OhttpKeyService* ohttp_key_service,
    WebUIDelegate* webui_delegate)
    : get_network_context_(std::move(get_network_context)),
      cache_manager_(cache_manager),
      ohttp_key_service_(ohttp_key_service),
      backoff_operator_(std::make_unique<BackoffOperator>(
          /*num_failures_to_enforce_backoff=*/kNumFailuresToEnforceBackoff,
          /*min_backoff_reset_duration_in_seconds=*/
          kMinBackOffResetDurationInSeconds,
          /*max_backoff_reset_duration_in_seconds=*/
          kMaxBackOffResetDurationInSeconds)),
      webui_delegate_(webui_delegate) {}

HashRealTimeService::~HashRealTimeService() = default;

// static
bool HashRealTimeService::CanCheckUrl(const GURL& url) {
  if (VerdictCacheManager::has_artificial_cached_url()) {
    return true;
  }
  return hash_realtime_utils::CanCheckUrl(url);
}

HashRealTimeService::SBThreatInfo::SBThreatInfo(SBThreatType threat_type,
                                                int num_full_hash_matches)
    : threat_type(threat_type), num_full_hash_matches(num_full_hash_matches) {}

// static
HashRealTimeService::SBThreatInfo HashRealTimeService::DetermineSBThreatInfo(
    const GURL& url,
    const std::vector<V5::FullHash>& result_full_hashes) {
  std::vector<std::string> url_full_hashes_vector;
  V4ProtocolManagerUtil::UrlToFullHashes(url, &url_full_hashes_vector);
  std::set<std::string> url_full_hashes(url_full_hashes_vector.begin(),
                                        url_full_hashes_vector.end());
  SBThreatType sb_threat_type = SBThreatType::SB_THREAT_TYPE_SAFE;
  int threat_severity = kLeastSeverity;
  int num_full_hash_matches = 0;
  for (const auto& hash_proto : result_full_hashes) {
    auto it = url_full_hashes.find(hash_proto.full_hash());
    if (url_full_hashes.end() != it) {
      for (const auto& detail : hash_proto.full_hash_details()) {
        if (hash_realtime_utils::IsHashDetailRelevant(detail)) {
          ++num_full_hash_matches;
          if (IsHashDetailMoreSevere(detail, threat_severity)) {
            threat_severity = GetThreatSeverity(detail);
            sb_threat_type = MapFullHashDetailToSbThreatType(detail);
          }
        }
      }
    }
  }
  return SBThreatInfo(sb_threat_type, num_full_hash_matches);
}
int HashRealTimeService::GetThreatSeverity(
    const V5::FullHash::FullHashDetail& detail) {
  // These values should be consistent with the ones in GetThreatSeverity in
  // v4_local_database_manager.cc.
  if (base::Contains(detail.attributes(), V5::ThreatAttribute::CANARY)) {
    // ThreatAttribute::CANARY should be equivalent to SUSPICIOUS.
    return 4;
  }

  switch (detail.threat_type()) {
    case V5::ThreatType::MALWARE:
    case V5::ThreatType::SOCIAL_ENGINEERING:
      return 0;
    case V5::ThreatType::UNWANTED_SOFTWARE:
      return 1;
    case V5::ThreatType::TRICK_TO_BILL:
      return 15;
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      NOTREACHED_IN_MIGRATION()
          << "Unexpected ThreatType encountered: " << detail.threat_type();
      return kLeastSeverity;
  }
}
bool HashRealTimeService::IsHashDetailMoreSevere(
    const V5::FullHash::FullHashDetail& detail,
    int baseline_severity) {
  auto candidate_severity = GetThreatSeverity(detail);
  return candidate_severity < baseline_severity;
}

std::set<std::string> HashRealTimeService::GetHashPrefixesSet(
    const GURL& url) const {
  std::vector<std::string> full_hashes;
  V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  std::set<std::string> hash_prefixes;
  for (const auto& full_hash : full_hashes) {
    auto hash_prefix = hash_realtime_utils::GetHashPrefix(full_hash);
    hash_prefixes.insert(hash_prefix);
  }
  return hash_prefixes;
}

void HashRealTimeService::SearchCache(
    std::set<std::string> hash_prefixes,
    std::vector<std::string>* out_missing_hash_prefixes,
    std::vector<V5::FullHash>* out_cached_full_hashes) const {
  SCOPED_UMA_HISTOGRAM_TIMER("SafeBrowsing.HPRT.GetCache.Time");
  auto cached_results =
      cache_manager_
          ? cache_manager_->GetCachedHashPrefixRealTimeLookupResults(
                hash_prefixes)
          : std::unordered_map<std::string, std::vector<V5::FullHash>>();
  for (const auto& hash_prefix : hash_prefixes) {
    auto cached_result_it = cached_results.find(hash_prefix);
    if (cached_result_it != cached_results.end()) {
      // If in the cache, keep track of associated full hashes to merge them
      // with the response results later.
      for (const auto& cached_full_hash : cached_result_it->second) {
        out_cached_full_hashes->push_back(cached_full_hash);
      }
    } else {
      // If not in the cache, add the prefix to hash prefixes to request.
      out_missing_hash_prefixes->push_back(hash_prefix);
    }
  }
}

void HashRealTimeService::StartLookup(
    const GURL& url,
    HPRTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartLookupInternal(
      url, std::make_unique<LookupCompleter>(std::move(response_callback),
                                             std::move(callback_task_runner)));
}

void HashRealTimeService::StartLookupInternal(
    const GURL& url,
    std::unique_ptr<LookupCompleter> lookup_completer) {
  DCHECK(url.is_valid());

  // If |Shutdown| has been called, return early.
  if (is_shutdown_) {
    return;
  }

  // Search local cache.
  std::vector<std::string> hash_prefixes_to_request;
  std::vector<V5::FullHash> cached_full_hashes;
  SearchCache(GetHashPrefixesSet(url), &hash_prefixes_to_request,
              &cached_full_hashes);
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.CacheHitAllPrefixes",
                            hash_prefixes_to_request.empty());
  // If all the prefixes are in the cache, no need to send a request. Return
  // early with the cached results.
  if (hash_prefixes_to_request.empty()) {
    SBThreatInfo sb_threat_info =
        DetermineSBThreatInfo(url, cached_full_hashes);
    LogThreatInfoSize(sb_threat_info.num_full_hash_matches,
                      /*is_source_local_cache=*/true);
    lookup_completer->CompleteLookup(/*is_lookup_successful=*/true,
                                     sb_threat_info.threat_type,
                                     OperationOutcome::kResultInLocalCache);
    return;
  }

  // If the service is in backoff mode, don't send a request.
  bool in_backoff = backoff_operator_->IsInBackoffMode();
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.BackoffState", in_backoff);
  if (in_backoff) {
    lookup_completer->CompleteLookup(/*is_lookup_successful=*/false,
                                     /*sb_threat_type=*/std::nullopt,
                                     OperationOutcome::kServiceInBackoffMode);
    return;
  }

  // Prepare request.
  auto request = std::make_unique<V5::SearchHashesRequest>();
  for (const auto& hash_prefix : hash_prefixes_to_request) {
    request->add_hash_prefixes(hash_prefix);
  }
  base::UmaHistogramCounts100("SafeBrowsing.HPRT.Request.CountOfPrefixes",
                              hash_prefixes_to_request.size());

  // Send OHTTP request.
  ohttp_key_service_->GetOhttpKey(base::BindOnce(
      &HashRealTimeService::OnGetOhttpKey, weak_factory_.GetWeakPtr(),
      std::move(request), url, std::move(hash_prefixes_to_request),
      std::move(cached_full_hashes), base::TimeTicks::Now(),
      std::move(lookup_completer)));
}

void HashRealTimeService::OnGetOhttpKey(
    std::unique_ptr<V5::SearchHashesRequest> request,
    const GURL& url,
    const std::vector<std::string>& hash_prefixes_in_request,
    std::vector<V5::FullHash> result_full_hashes,
    base::TimeTicks request_start_time,
    std::unique_ptr<LookupCompleter> lookup_completer,
    std::optional<std::string> key) {
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.HasOhttpKey", key.has_value());
  if (!key.has_value()) {
    lookup_completer->CompleteLookup(/*is_lookup_successful=*/false,
                                     /*sb_threat_type=*/std::nullopt,
                                     OperationOutcome::kOhttpKeyFetchFailed);
    return;
  }
  // Construct OHTTP request.
  network::mojom::ObliviousHttpRequestPtr ohttp_request =
      network::mojom::ObliviousHttpRequest::New();
  GURL relay_url = GURL(kHashPrefixRealTimeLookupsRelayUrl.Get());
  ohttp_request->relay_url = relay_url;
  ohttp_request->traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      GetTrafficAnnotationTagForOhttp());
  ohttp_request->key_config = key.value();
  ohttp_request->resource_url = GURL(GetResourceUrl(request.get()));
  ohttp_request->method = net::HttpRequestHeaders::kGetMethod;
  ohttp_request->timeout_duration =
      base::Seconds(kLookupTimeoutDurationInSeconds);

  mojo::PendingReceiver<network::mojom::ObliviousHttpClient> pending_receiver;
  get_network_context_.Run()->GetViaObliviousHttp(
      std::move(ohttp_request),
      pending_receiver.InitWithNewPipeAndPassRemote());
  // The following |webui_delegate_| call is to log this HPRT lookup request on
  // any open chrome://safe-browsing pages.
  std::optional<int> webui_delegate_token =
      webui_delegate_ ? webui_delegate_->AddToHPRTLookupPings(
                            request.get(), relay_url.spec(), key.value())
                      : std::nullopt;
  ohttp_client_receivers_.Add(
      std::make_unique<ObliviousHttpClient>(base::BindOnce(
          &HashRealTimeService::OnOhttpComplete, weak_factory_.GetWeakPtr(),
          url, std::move(hash_prefixes_in_request),
          std::move(result_full_hashes), request_start_time,
          std::move(lookup_completer), key.value(), webui_delegate_token)),
      std::move(pending_receiver));
}

void HashRealTimeService::OnOhttpComplete(
    const GURL& url,
    const std::vector<std::string>& hash_prefixes_in_request,
    std::vector<V5::FullHash> result_full_hashes,
    base::TimeTicks request_start_time,
    std::unique_ptr<LookupCompleter> lookup_completer,
    std::string ohttp_key,
    std::optional<int> webui_delegate_token,
    const std::optional<std::string>& response_body,
    int net_error,
    int response_code,
    scoped_refptr<net::HttpResponseHeaders> headers,
    bool ohttp_client_destructed_early) {
  ohttp_key_service_->NotifyLookupResponse(ohttp_key, response_code, headers);

  auto response_body_ptr =
      std::make_unique<std::string>(response_body.value_or(""));
  OnURLLoaderComplete(url, std::move(hash_prefixes_in_request),
                      std::move(result_full_hashes), request_start_time,
                      std::move(lookup_completer), std::move(response_body_ptr),
                      net_error, response_code, webui_delegate_token,
                      ohttp_client_destructed_early);
}

void HashRealTimeService::OnURLLoaderComplete(
    const GURL& url,
    const std::vector<std::string>& hash_prefixes_in_request,
    std::vector<V5::FullHash> result_full_hashes,
    base::TimeTicks request_start_time,
    std::unique_ptr<LookupCompleter> lookup_completer,
    std::unique_ptr<std::string> response_body,
    int net_error,
    int response_code,
    std::optional<int> webui_delegate_token,
    bool ohttp_client_destructed_early) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta network_time = base::TimeTicks::Now() - request_start_time;
  base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time", network_time);
  RecordHttpResponseOrErrorCode("SafeBrowsing.HPRT.Network.Result", net_error,
                                response_code);
  if (net_error == net::ERR_INTERNET_DISCONNECTED) {
    base::UmaHistogramSparse(
        "SafeBrowsing.HPRT.Network.HttpResponseCode.InternetDisconnected",
        response_code);
  }
  if (net_error == net::ERR_NETWORK_CHANGED) {
    base::UmaHistogramSparse(
        "SafeBrowsing.HPRT.Network.HttpResponseCode.NetworkChanged",
        response_code);
  }
  if (net_error == net::ERR_FAILED) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.HPRT.FailedNetResultIsFromEarlyOhttpClientDestruct",
        ohttp_client_destructed_early);
  }
  if (net_error == net::ERR_NAME_NOT_RESOLVED) {
    base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time.NameNotResolved",
                            network_time);
  }
  if (net_error == net::ERR_CONNECTION_CLOSED) {
    base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time.ConnectionClosed",
                            network_time);
  }

  base::expected<std::unique_ptr<V5::SearchHashesResponse>, OperationOutcome>
      response = ParseResponseAndUpdateBackoff(net_error, response_code,
                                               std::move(response_body),
                                               hash_prefixes_in_request);
  std::optional<SBThreatType> sb_threat_type;
  bool is_lookup_successful = response.has_value();
  if (is_lookup_successful) {
    if (cache_manager_) {
      cache_manager_->CacheHashPrefixRealTimeLookupResults(
          hash_prefixes_in_request,
          std::vector<V5::FullHash>(response.value()->full_hashes().begin(),
                                    response.value()->full_hashes().end()),
          response.value()->cache_duration());
    }

    // Merge together the results from the cache and from the response.
    for (const auto& response_hash : response.value()->full_hashes()) {
      result_full_hashes.push_back(response_hash);
    }
    SBThreatInfo sb_threat_info =
        DetermineSBThreatInfo(url, result_full_hashes);
    sb_threat_type = sb_threat_info.threat_type;
    LogThreatInfoSize(sb_threat_info.num_full_hash_matches,
                      /*is_source_local_cache=*/false);
  }

  lookup_completer->CompleteLookup(
      is_lookup_successful, sb_threat_type,
      response.error_or(OperationOutcome::kSuccess));
  if (webui_delegate_ && is_lookup_successful &&
      webui_delegate_token.has_value()) {
    // The following |webui_delegate_| call is to log this HPRT lookup response
    // on any open chrome://safe-browsing pages.
    webui_delegate_->AddToHPRTLookupResponses(webui_delegate_token.value(),
                                              response.value().get());
  }
}

base::expected<std::unique_ptr<V5::SearchHashesResponse>,
               HashRealTimeService::OperationOutcome>
HashRealTimeService::ParseResponseAndUpdateBackoff(
    int net_error,
    int response_code,
    std::unique_ptr<std::string> response_body,
    const std::vector<std::string>& requested_hash_prefixes) const {
  auto response =
      ParseResponse(net_error, response_code, std::move(response_body),
                    requested_hash_prefixes);
  if (response.has_value()) {
    backoff_operator_->ReportSuccess();
  } else if (response.error() != OperationOutcome::kRetriableError) {
    bool newly_in_backoff_mode = backoff_operator_->ReportError();
    if (newly_in_backoff_mode) {
      RecordHttpResponseOrErrorCode(
          "SafeBrowsing.HPRT.Network.Result.WhenEnteringBackoff", net_error,
          response_code);
    }
  }
  return response;
}

void HashRealTimeService::RemoveUnmatchedFullHashes(
    std::unique_ptr<V5::SearchHashesResponse>& response,
    const std::vector<std::string>& requested_hash_prefixes) const {
  size_t initial_full_hashes_count = response->full_hashes_size();
  std::set<std::string> requested_hash_prefixes_set(
      requested_hash_prefixes.begin(), requested_hash_prefixes.end());
  auto* mutable_full_hashes = response->mutable_full_hashes();
  mutable_full_hashes->erase(
      std::remove_if(
          mutable_full_hashes->begin(), mutable_full_hashes->end(),
          [requested_hash_prefixes_set](const V5::FullHash& full_hash) {
            return !base::Contains(
                requested_hash_prefixes_set,
                hash_realtime_utils::GetHashPrefix(full_hash.full_hash()));
          }),
      mutable_full_hashes->end());
  size_t final_full_hashes_count = response->full_hashes_size();
  base::UmaHistogramBoolean(
      "SafeBrowsing.HPRT.FoundUnmatchedFullHashes",
      initial_full_hashes_count != final_full_hashes_count);
}

void HashRealTimeService::RemoveFullHashDetailsWithInvalidEnums(
    std::unique_ptr<V5::SearchHashesResponse>& response) const {
  for (int i = 0; i < response->full_hashes_size(); ++i) {
    auto* mutable_details =
        response->mutable_full_hashes(i)->mutable_full_hash_details();
    mutable_details->erase(
        std::remove_if(mutable_details->begin(), mutable_details->end(),
                       [](const V5::FullHash::FullHashDetail& detail) {
                         if (!V5::ThreatType_IsValid(detail.threat_type())) {
                           return true;
                         }
                         for (const auto& attribute : detail.attributes()) {
                           if (!V5::ThreatAttribute_IsValid(attribute)) {
                             return true;
                           }
                         }
                         return false;
                       }),
        mutable_details->end());
  }
}

base::expected<std::unique_ptr<V5::SearchHashesResponse>,
               HashRealTimeService::OperationOutcome>
HashRealTimeService::ParseResponse(
    int net_error,
    int response_code,
    std::unique_ptr<std::string> response_body,
    const std::vector<std::string>& requested_hash_prefixes) const {
  if (net_error != net::OK &&
      net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    return base::unexpected(ErrorIsRetriable(net_error, response_code)
                                ? OperationOutcome::kRetriableError
                                : OperationOutcome::kNetworkError);
  }
  if (response_code != net::HTTP_OK) {
    return base::unexpected(OperationOutcome::kHttpError);
  }
  CHECK_EQ(net::OK, net_error);
  auto response = std::make_unique<V5::SearchHashesResponse>();
  if (!response->ParseFromString(*response_body)) {
    return base::unexpected(OperationOutcome::kParseError);
  }
  if (!response->has_cache_duration()) {
    return base::unexpected(OperationOutcome::kNoCacheDurationError);
  }
  for (const auto& full_hash : response->full_hashes()) {
    if (full_hash.full_hash().length() !=
        hash_realtime_utils::kFullHashLength) {
      return base::unexpected(OperationOutcome::kIncorrectFullHashLengthError);
    }
  }
  RemoveUnmatchedFullHashes(response, requested_hash_prefixes);
  RemoveFullHashDetailsWithInvalidEnums(response);
  return std::move(response);
}

std::string HashRealTimeService::GetResourceUrl(
    V5::SearchHashesRequest* request) const {
  std::string request_data, request_base64;
  request->SerializeToString(&request_data);
  base::Base64UrlEncode(request_data,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &request_base64);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string url = base::StringPrintf(
      "https://safebrowsing.googleapis.com/v5/hashes:search"
      "?$req=%s&$ct=application/x-protobuf",
      request_base64.c_str());
  auto api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        base::EscapeQueryParamValue(api_key, true).c_str());
  }
  return url;
}

void HashRealTimeService::Shutdown() {
  is_shutdown_ = true;
  weak_factory_.InvalidateWeakPtrs();

  // Clear references to other KeyedServices.
  cache_manager_ = nullptr;
  ohttp_key_service_ = nullptr;
}

base::WeakPtr<HashRealTimeService> HashRealTimeService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

net::NetworkTrafficAnnotationTag
HashRealTimeService::GetTrafficAnnotationTagForOhttp() const {
  return net::DefineNetworkTrafficAnnotation(
      "safe_browsing_hashprefix_realtime_lookup_ohttp",
      R"(
  semantics {
    sender: "Safe Browsing"
    description:
      "When Safe Browsing can't detect that a URL is safe based on its "
      "local database, it sends partial hashes of the URL to Google to check "
      "whether to show a warning to the user. These partial hashes do not "
      "expose the URL to Google. The partial hashes are sent to a proxy via "
      "Oblivious HTTP first and then relayed to Google. The source of the "
      "requests (e.g. IP address) is anonymized to Google."
    trigger:
      "When a main frame URL fails to match the local hash-prefix "
      "database of known safe URLs and a valid result from a prior "
      "lookup is not already cached, this will be sent."
    data:
        "The 32-bit hash prefixes of the URL that did not match the local "
        " safelist. The URL itself is not sent."
    destination: PROXIED_GOOGLE_OWNED_SERVICE
    internal {
      contacts {
        email: "thefrog@chromium.org"
      }
      contacts {
        email: "chrome-counter-abuse-alerts@google.com"
      }
    }
    user_data {
      type: NONE
    }
    last_reviewed: "2023-04-20"
  }
  policy {
    cookies_allowed: NO
    setting:
      "Users can disable Safe Browsing by checking 'No protection' in Chromium "
      "settings under Security > Safe Browsing. The feature is enabled by "
      "default."
    chrome_policy {
      SafeBrowsingProtectionLevel {
        policy_options {mode: MANDATORY}
        SafeBrowsingProtectionLevel: 0
      }
    }
    chrome_policy {
      SafeBrowsingEnabled {
        policy_options {mode: MANDATORY}
        SafeBrowsingEnabled: false
      }
    }
    chrome_policy {
      SafeBrowsingProxiedRealTimeChecksAllowed {
        policy_options {mode: MANDATORY}
        SafeBrowsingProxiedRealTimeChecksAllowed: false
      }
    }
    deprecated_policies: "SafeBrowsingEnabled"
  })");
}

HashRealTimeService::LookupCompleter::LookupCompleter(
    HPRTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner)
    : response_callback_(std::move(response_callback)),
      response_callback_task_runner_(std::move(response_callback_task_runner)) {
}

HashRealTimeService::LookupCompleter::~LookupCompleter() = default;

void HashRealTimeService::LookupCompleter::CompleteLookup(
    bool is_lookup_successful,
    std::optional<SBThreatType> sb_threat_type,
    OperationOutcome operation_outcome) {
  CHECK(!is_call_complete_);
  is_call_complete_ = true;
  base::UmaHistogramEnumeration("SafeBrowsing.HPRT.OperationOutcome",
                                operation_outcome);
  response_callback_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(response_callback_),
                                is_lookup_successful, sb_threat_type));
}

}  // namespace safe_browsing
