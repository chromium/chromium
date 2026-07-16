// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_util.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

const size_t kNumFailuresToEnforceBackoff = 3;
const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.

const size_t kLookupTimeoutDurationInSeconds = 3;

void LogThreatInfoSize(int num_full_hash_matches) {
  base::UmaHistogramCounts100("SafeBrowsing.HPRT.ThreatInfoSize",
                              num_full_hash_matches);
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
                              bool)>;

  explicit ObliviousHttpClient(OnCompletedCallback callback)
      : callback_(std::move(callback)) {}

  ~ObliviousHttpClient() override {
    if (!called_) {
      std::move(callback_).Run(std::nullopt, net::ERR_FAILED,
                               /*response_code=*/0,
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
    V5SearchHashesCache* cache,
    OhttpKeyService* ohttp_key_service,
    WebUIDelegate* webui_delegate)
    : get_network_context_(std::move(get_network_context)),
      cache_(cache),
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
  if (V5SearchHashesCache::has_artificial_cached_url()) {
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
  SBProtocolManagerUtil::UrlToFullHashes(url, &url_full_hashes_vector);
  std::set<std::string> url_full_hashes(url_full_hashes_vector.begin(),
                                        url_full_hashes_vector.end());

  std::vector<const V5::FullHash::FullHashDetail*> filtered_details;
  for (const auto& match : result_full_hashes) {
    if (!url_full_hashes.contains(match.full_hash())) {
      // Filter out result full hashes that don't match the full hashes of our
      // URL.
      continue;
    }
    for (const auto& detail : match.full_hash_details()) {
      if (hash_realtime_utils::IsHashDetailRelevant(detail)) {
        // Filter out result full hash details that are not relevant for HPRT
        // lookups.
        filtered_details.push_back(&detail);
      }
    }
  }

  safe_browsing::v5_search_hashes_util::ThreatResult result =
      safe_browsing::v5_search_hashes_util::DetermineMostSevereThreat(
          filtered_details);
  return SBThreatInfo(result.threat_type, filtered_details.size());
}

std::set<std::string> HashRealTimeService::GetHashPrefixesSet(
    const GURL& url) const {
  std::vector<std::string> full_hashes;
  SBProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  std::set<std::string> hash_prefixes;
  for (const auto& full_hash : full_hashes) {
    auto hash_prefix = SBProtocolManagerUtil::GetHashPrefix(full_hash);
    hash_prefixes.insert(hash_prefix);
  }
  return hash_prefixes;
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
  {
    SCOPED_UMA_HISTOGRAM_TIMER("SafeBrowsing.HPRT.GetCache.Time");
    safe_browsing::v5_search_hashes_util::SearchCache(
        cache_, GetHashPrefixesSet(url), &hash_prefixes_to_request,
        &cached_full_hashes);
  }
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.CacheHitAllPrefixes",
                            hash_prefixes_to_request.empty());
  // If all the prefixes are in the cache, no need to send a request. Return
  // early with the cached results.
  if (hash_prefixes_to_request.empty()) {
    SBThreatInfo sb_threat_info =
        DetermineSBThreatInfo(url, cached_full_hashes);
    LogThreatInfoSize(sb_threat_info.num_full_hash_matches);
    lookup_completer->CompleteLookup(/*is_lookup_successful=*/true,
                                     sb_threat_info.threat_type,
                                     OperationOutcome::kResultInLocalCache);
    return;
  }

  // If the service is in backoff mode, don't send a request.
  bool in_backoff = backoff_operator_->IsInBackoffMode();
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.BackoffState", in_backoff);
  if (in_backoff) {
    CHECK(outcome_details_when_entered_backoff_.has_value());
    base::UmaHistogramEnumeration(
        "SafeBrowsing.HPRT.BackoffEnabled.OperationOutcome.WhenEnteredBackoff",
        outcome_details_when_entered_backoff_->operation_outcome);
    RecordHttpResponseOrErrorCode(
        "SafeBrowsing.HPRT.BackoffEnabled.Network.Result.WhenEnteredBackoff",
        outcome_details_when_entered_backoff_->net_error,
        outcome_details_when_entered_backoff_->response_code);
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
  ohttp_request->resource_url =
      GURL(safe_browsing::v5_search_hashes_util::GetResourceUrl(request.get()));
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
          base::TimeTicks::Now(), std::move(lookup_completer), key.value(),
          webui_delegate_token)),
      std::move(pending_receiver));
}

void HashRealTimeService::OnOhttpComplete(
    const GURL& url,
    const std::vector<std::string>& hash_prefixes_in_request,
    std::vector<V5::FullHash> result_full_hashes,
    base::TimeTicks key_request_start_time,
    base::TimeTicks hash_request_start_time,
    std::unique_ptr<LookupCompleter> lookup_completer,
    std::string ohttp_key,
    std::optional<int> webui_delegate_token,
    const std::optional<std::string>& response_body,
    int net_error,
    int response_code,
    bool ohttp_client_destructed_early) {
  ohttp_key_service_->NotifyLookupResponse(ohttp_key, response_code);

  auto response_body_ptr =
      std::make_unique<std::string>(response_body.value_or(""));
  OnURLLoaderComplete(url, std::move(hash_prefixes_in_request),
                      std::move(result_full_hashes), key_request_start_time,
                      hash_request_start_time, std::move(lookup_completer),
                      std::move(response_body_ptr), net_error, response_code,
                      webui_delegate_token, ohttp_client_destructed_early);
}

void HashRealTimeService::OnURLLoaderComplete(
    const GURL& url,
    const std::vector<std::string>& hash_prefixes_in_request,
    std::vector<V5::FullHash> result_full_hashes,
    base::TimeTicks key_request_start_time,
    base::TimeTicks hash_request_start_time,
    std::unique_ptr<LookupCompleter> lookup_completer,
    std::unique_ptr<std::string> response_body,
    int net_error,
    int response_code,
    std::optional<int> webui_delegate_token,
    bool ohttp_client_destructed_early) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta full_request_duration = now - key_request_start_time;
  base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time",
                          full_request_duration);
  base::TimeDelta hash_request_duration = now - hash_request_start_time;
  base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time.Hash",
                          hash_request_duration);
  if (net_error == net::OK && response_code == net::HTTP_OK && response_body &&
      !response_body->empty()) {
    base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time.Success",
                            full_request_duration);
    base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time.Hash.Success",
                            hash_request_duration);
  } else {
    base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time.Failure",
                            full_request_duration);
    base::UmaHistogramTimes("SafeBrowsing.HPRT.Network.Time.Hash.Failure",
                            hash_request_duration);
  }
  RecordHttpResponseOrErrorCode("SafeBrowsing.HPRT.Network.Result", net_error,
                                response_code);
  if (net_error == net::ERR_FAILED) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.HPRT.FailedNetResultIsFromEarlyOhttpClientDestruct",
        ohttp_client_destructed_early);
  }

  base::expected<std::unique_ptr<V5::SearchHashesResponse>, OperationOutcome>
      response = ParseResponseAndUpdateBackoff(net_error, response_code,
                                               std::move(response_body),
                                               hash_prefixes_in_request);
  std::optional<SBThreatType> sb_threat_type;
  bool is_lookup_successful = response.has_value();
  if (is_lookup_successful) {
    if (cache_) {
      cache_->CacheSearchHashesResponse(
          hash_prefixes_in_request,
          std::vector<V5::FullHash>(response.value()->full_hashes().begin(),
                                    response.value()->full_hashes().end()),
          response.value()->cache_duration());
    }

    // Merge together the results from the cache and from the response.
    result_full_hashes.insert(result_full_hashes.end(),
                              response.value()->full_hashes().begin(),
                              response.value()->full_hashes().end());
    SBThreatInfo sb_threat_info =
        DetermineSBThreatInfo(url, result_full_hashes);
    sb_threat_type = sb_threat_info.threat_type;
    LogThreatInfoSize(sb_threat_info.num_full_hash_matches);
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
    const std::vector<std::string>& requested_hash_prefixes) {
  auto response =
      ParseResponse(net_error, response_code, std::move(response_body),
                    requested_hash_prefixes);
  if (response.has_value()) {
    backoff_operator_->ReportSuccess();
    outcome_details_when_entered_backoff_.reset();
  } else if (response.error() != OperationOutcome::kRetriableError) {
    bool newly_in_backoff_mode = backoff_operator_->ReportError();
    if (newly_in_backoff_mode) {
      RecordHttpResponseOrErrorCode(
          "SafeBrowsing.HPRT.Network.Result.WhenEnteringBackoff", net_error,
          response_code);
      base::UmaHistogramEnumeration(
          "SafeBrowsing.HPRT.OperationOutcome.WhenEnteringBackoff",
          response.error());
      outcome_details_when_entered_backoff_ = {response.error(), net_error,
                                               response_code};
    }
  }
  return response;
}


base::expected<std::unique_ptr<V5::SearchHashesResponse>,
               HashRealTimeService::OperationOutcome>
HashRealTimeService::ParseResponse(
    int net_error,
    int response_code,
    std::unique_ptr<std::string> response_body,
    const std::vector<std::string>& requested_hash_prefixes) const {
  auto parse_info = safe_browsing::v5_search_hashes_util::ParseResponse(
      net_error, response_code, *response_body, requested_hash_prefixes);
  if (!parse_info.has_value()) {
    switch (parse_info.error()) {
      case safe_browsing::v5_search_hashes_util::ParseFailure::kNetworkError:
        return base::unexpected(OperationOutcome::kNetworkError);
      case safe_browsing::v5_search_hashes_util::ParseFailure::kHttpError:
        return base::unexpected(OperationOutcome::kHttpError);
      case safe_browsing::v5_search_hashes_util::ParseFailure::kRetriableError:
        return base::unexpected(OperationOutcome::kRetriableError);
      case safe_browsing::v5_search_hashes_util::ParseFailure::kParseError:
        return base::unexpected(OperationOutcome::kParseError);
      case safe_browsing::v5_search_hashes_util::ParseFailure::
          kNoCacheDurationError:
        return base::unexpected(OperationOutcome::kNoCacheDurationError);
      case safe_browsing::v5_search_hashes_util::ParseFailure::
          kIncorrectFullHashLengthError:
        return base::unexpected(
            OperationOutcome::kIncorrectFullHashLengthError);
    }
  }
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.FoundUnmatchedFullHashes",
                            parse_info->found_unmatched_full_hashes);
  return std::make_unique<V5::SearchHashesResponse>(
      std::move(parse_info->response));
}

void HashRealTimeService::Shutdown() {
  is_shutdown_ = true;
  weak_factory_.InvalidateWeakPtrs();

  // Clear references to other KeyedServices.
  cache_ = nullptr;
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
