// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_util.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

// Backoff policy for V5 GetHash requests.
// With initial_delay_ms = 30 minutes, multiply_factor = 2.0, and
// jitter_factor = 0.5, the backoff delay increases exponentially with
// subsequent failures and has random jitter in [0.5 * base, base]:
//  - 1st failure: 15 min - 30 min
//  - 2nd failure: 30 min to 1 hour
//  - 3rd failure: 1 hour to 2 hours
//  - 4th failure: 2 hours to 4 hours
//  - Subsequent failures continue doubling up to the 24 hour maximum backoff
//    cap.
const net::BackoffEntry::Policy kV5GetHashBackoffPolicy = {
    0,                    // num_errors_to_ignore
    30 * 60 * 1000,       // initial_delay_ms (30 minutes)
    2.0,                  // multiply_factor (exponentially increases delays
                          // by 2x for each new failure)
    0.5,                  // jitter_factor (randomly reduces the delay by up
                          // to 50%)
    24 * 60 * 60 * 1000,  // maximum_backoff_ms (24 hours)
    -1,                   // entry_lifetime_ms
    false,                // always_use_initial_delay
};

void RecordNumRequestsSkippedDuringBackoff(size_t count) {
  base::UmaHistogramCounts100(
      "SafeBrowsing.V5GetHash.NumRequestsSkippedDuringBackoff", count);
  base::UmaHistogramCounts100("SafeBrowsing.SBGetHash.Result.BackoffErrorCount",
                              count);
}

bool IsHashDetailRelevantForLocalChecks(
    const V5::FullHash::FullHashDetail& detail) {
  bool has_canary = false;
  bool has_frame_only = false;

  for (int attribute_value : detail.attributes()) {
    // LINT.IfChange(ThreatAttribute)
    switch (attribute_value) {
      case V5::ThreatAttribute::CANARY:
        has_canary = true;
        break;
      case V5::ThreatAttribute::FRAME_ONLY:
        has_frame_only = true;
        break;
      case V5::ThreatAttribute::THREAT_ATTRIBUTE_UNSPECIFIED:
        break;
        // LINT.ThenChange(//components/safe_browsing/core/common/proto/safebrowsingv5.proto:ThreatAttribute)
      default:
        // Using "default" because exhaustive switch statements are not
        // recommended for proto3 enums.
        break;
    }
  }

#if BUILDFLAG(IS_IOS)
  // iOS doesn't support CANARY threat attribute.
  if (has_canary) {
    return false;
  }
#endif

  // CANARY and FRAME_ONLY should not be set at the same time.
  if (has_canary && has_frame_only) {
    return false;
  }

  // CANARY should only be attached with SOCIAL_ENGINEERING,
  // ABUSIVE_EXPERIENCE_VIOLATION, BETTER_ADS_VIOLATION.
  // LINT.IfChange(ThreatType)
  switch (detail.threat_type()) {
    case V5::ThreatType::SOCIAL_ENGINEERING:
    case V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION:
    case V5::ThreatType::BETTER_ADS_VIOLATION:
      // These types support both warning (CANARY) and enforcement.
      return true;

    case V5::ThreatType::MALWARE:
    case V5::ThreatType::MALICIOUS_BINARY:
    case V5::ThreatType::UNWANTED_SOFTWARE:
    case V5::ThreatType::TRICK_TO_BILL:
    case V5::ThreatType::NOTIFICATION_ABUSE:
      // These types only support enforcement; CANARY is invalid.
      return !has_canary;

    case V5::ThreatType::THREAT_TYPE_UNSPECIFIED:
    case V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION:
    case V5::ThreatType::SUBRESOURCE_FILTER:
      // These types are not supported/relevant for V5 local DB queries.
      return false;
      // LINT.ThenChange(//components/safe_browsing/core/common/proto/safebrowsingv5.proto:ThreatType)
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      return false;
  }
}

}  // namespace

V5GetHashProtocolManager::V5GetHashProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config,
    V5SearchHashesCache* cache)
    : url_loader_factory_(url_loader_factory),
      config_(config),
      cache_(cache),
      backoff_entry_(
          std::make_unique<net::BackoffEntry>(&kV5GetHashBackoffPolicy)) {}

V5GetHashProtocolManager::~V5GetHashProtocolManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void V5GetHashProtocolManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_loaders_.clear();
}

void V5GetHashProtocolManager::GetFullHashes(
    const std::map<FullHashStr, std::vector<SBThreatType>>&
        full_hash_to_threat_types,
    FullHashCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!full_hash_to_threat_types.empty());

  std::vector<std::string> hash_prefixes_to_request;
  std::vector<V5::FullHash> cached_full_hashes;

  // Determine which hash prefixes we are interested in.
  std::set<std::string> unique_prefixes;
  for (const auto& entry : full_hash_to_threat_types) {
    unique_prefixes.insert(SBProtocolManagerUtil::GetHashPrefix(entry.first));
  }

  // Check the local cache to see if any prefixes already have results.
  safe_browsing::v5_search_hashes_util::SearchCache(
      cache_.get(), unique_prefixes, &hash_prefixes_to_request,
      &cached_full_hashes);

  bool cache_hit_all = hash_prefixes_to_request.empty();
  base::UmaHistogramBoolean("SafeBrowsing.V5GetHash.CacheHitAllPrefixes",
                            cache_hit_all);
  base::UmaHistogramBoolean("SafeBrowsing.SBGetHash.CacheHitAllPrefixes",
                            cache_hit_all);
  // All results are in the cache so there's no need for a network request.
  // Return early.
  if (cache_hit_all) {
    ThreatTypeAndMetadata result = DetermineMostSevereThreatForLocalChecks(
        cached_full_hashes, full_hash_to_threat_types);
    CompleteLookup(std::move(callback), OperationOutcome::kLocalCacheHit,
                   result.threat_type, result.metadata);
    return;
  }

  // If the service is in backoff mode, don't send a request.
  if (backoff_entry_->ShouldRejectRequest()) {
    num_requests_skipped_for_backoff_++;
    CompleteLookup(std::move(callback), OperationOutcome::kBackoffError,
                   SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata());
    return;
  }

  base::UmaHistogramCounts100("SafeBrowsing.V5GetHash.Request.CountOfPrefixes",
                              hash_prefixes_to_request.size());
  base::UmaHistogramCounts100("SafeBrowsing.SBGetHash.Request.CountOfPrefixes",
                              hash_prefixes_to_request.size());

  // Build the request.
  V5::SearchHashesRequest request;
  for (const auto& prefix : hash_prefixes_to_request) {
    request.add_hash_prefixes(prefix);
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_v5_get_hash", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When Safe Browsing detects that a URL might be dangerous based on "
            "its local database, it sends partial hashes of that URL to Google "
            "to verify it before showing a warning to the user. These partial "
            "hashes do not expose the URL to Google."
          trigger:
            "When a resource URL matches the local hash-prefix database of "
            "potential threats (malware, phishing etc), and the full-hash "
            "result is not already cached, this will be sent."
          data:
             "The 32-bit hash prefix of any potentially bad URLs. The URLs "
             "themselves are not sent."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "thefrog@chromium.org"
            }
            contacts {
              email: "chrome-counter-abuse-alerts@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
          }
          last_reviewed: "2026-07-09"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable Safe Browsing by checking 'No protection' in "
            "Chromium settings under Security > Safe Browsing. The feature is "
            "enabled by default."
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
          deprecated_policies: "SafeBrowsingEnabled"
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL(safe_browsing::v5_search_hashes_util::GetResourceUrl(&request));
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // TODO(crbug.com/362791941): share with v5_update_protocol_manager
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kUserAgent,
      base::StrCat({config_.client_name, " ", config_.version}));

  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* loader = owned_loader.get();
  base::TimeTicks request_start_time = base::TimeTicks::Now();
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(
          &V5GetHashProtocolManager::OnURLLoaderComplete,
          weak_factory_.GetWeakPtr(), loader, full_hash_to_threat_types,
          std::move(hash_prefixes_to_request), std::move(cached_full_hashes),
          std::move(callback), request_start_time));

  pending_loaders_.insert(std::move(owned_loader));
}

void V5GetHashProtocolManager::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types,
    std::vector<std::string> requested_prefixes,
    std::vector<V5::FullHash> cached_full_hashes,
    FullHashCallback callback,
    base::TimeTicks request_start_time,
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<std::unique_ptr<network::SimpleURLLoader>,
           base::UniquePtrComparator>::iterator it =
      pending_loaders_.find(url_loader);
  CHECK(it != pending_loaders_.end()) << "Request not found";
  std::unique_ptr<network::SimpleURLLoader> loader_to_delete =
      std::move(pending_loaders_.extract(it).value());

  base::TimeDelta request_duration =
      base::TimeTicks::Now() - request_start_time;
  base::UmaHistogramLongTimes("SafeBrowsing.V5GetHash.Network.Time",
                              request_duration);
  base::UmaHistogramLongTimes("SafeBrowsing.SBGetHash.Network.Time",
                              request_duration);

  // Parse the result.
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }
  int net_error = url_loader->NetError();
  RecordHttpResponseOrErrorCode("SafeBrowsing.V5GetHash.Network.Result",
                                net_error, response_code);
  RecordHttpResponseOrErrorCode("SafeBrowsing.SBGetHash.Network.Result",
                                net_error, response_code);

  base::expected<v5_search_hashes_util::ParseResultSuccess, OperationOutcome>
      parse_info = ParseResponseAndUpdateBackoff(net_error, response_code,
                                                 response_body.value_or(""),
                                                 requested_prefixes);

  // Return upon error.
  if (!parse_info.has_value()) {
    CompleteLookup(std::move(callback), parse_info.error(),
                   SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata());
    return;
  }

  const V5::SearchHashesResponse& response_proto = parse_info->response;
  std::vector<V5::FullHash> response_full_hashes(
      response_proto.full_hashes().begin(), response_proto.full_hashes().end());
  // Update the local cache.
  if (cache_) {
    cache_->CacheSearchHashesResponse(requested_prefixes, response_full_hashes,
                                      response_proto.cache_duration());
  }

  // Combine the results previously pulled from the cache and the results from
  // the network response.
  std::vector<V5::FullHash> matches = std::move(cached_full_hashes);
  matches.insert(matches.end(),
                 std::make_move_iterator(response_full_hashes.begin()),
                 std::make_move_iterator(response_full_hashes.end()));

  ThreatTypeAndMetadata result = DetermineMostSevereThreatForLocalChecks(
      matches, full_hash_to_threat_types);
  CompleteLookup(std::move(callback), OperationOutcome::kSuccess,
                 result.threat_type, result.metadata);
}

void V5GetHashProtocolManager::HandleBackoffResult(bool succeeded) {
  if (succeeded && backoff_entry_->failure_count() > 0) {
    RecordNumRequestsSkippedDuringBackoff(num_requests_skipped_for_backoff_);
    num_requests_skipped_for_backoff_ = 0;
  }
  backoff_entry_->InformOfRequest(succeeded);
}

base::expected<v5_search_hashes_util::ParseResultSuccess,
               V5GetHashProtocolManager::OperationOutcome>
V5GetHashProtocolManager::ParseResponseAndUpdateBackoff(
    int net_error,
    int response_code,
    const std::string& response_body,
    const std::vector<std::string>& requested_prefixes) {
  base::expected<v5_search_hashes_util::ParseResultSuccess,
                 v5_search_hashes_util::ParseFailure>
      parse_info = safe_browsing::v5_search_hashes_util::ParseResponse(
          net_error, response_code, response_body, requested_prefixes);

  if (parse_info.has_value()) {
    base::UmaHistogramBoolean("SafeBrowsing.V5GetHash.FoundUnmatchedFullHashes",
                              parse_info->found_unmatched_full_hashes);
    HandleBackoffResult(/*succeeded=*/true);
    return std::move(parse_info).value();
  }

  base::UmaHistogramEnumeration("SafeBrowsing.V5GetHash.ParseFailureReason",
                                parse_info.error());
  switch (parse_info.error()) {
    case safe_browsing::v5_search_hashes_util::ParseFailure::kNetworkError:
      HandleBackoffResult(/*succeeded=*/false);
      return base::unexpected(OperationOutcome::kNetworkError);
    case safe_browsing::v5_search_hashes_util::ParseFailure::kHttpError:
      HandleBackoffResult(/*succeeded=*/false);
      return base::unexpected(OperationOutcome::kHttpError);
    case safe_browsing::v5_search_hashes_util::ParseFailure::kRetriableError:
      // Retriable errors are ignored. Do not inform the backoff entry.
      return base::unexpected(OperationOutcome::kRetriableError);
    case safe_browsing::v5_search_hashes_util::ParseFailure::kParseError:
    case safe_browsing::v5_search_hashes_util::ParseFailure::
        kNoCacheDurationError:
    case safe_browsing::v5_search_hashes_util::ParseFailure::
        kIncorrectFullHashLengthError:
      // Parsing and validation errors do not count as server load issues,
      // so we treat them as successful requests to reset backoff.
      HandleBackoffResult(/*succeeded=*/true);
      return base::unexpected(OperationOutcome::kParseError);
  }
}

V5GetHashProtocolManager::ThreatTypeAndMetadata
V5GetHashProtocolManager::DetermineMostSevereThreatForLocalChecks(
    const std::vector<V5::FullHash>& matches,
    const std::map<FullHashStr, std::vector<SBThreatType>>&
        full_hash_to_threat_types) const {
  std::vector<const V5::FullHash::FullHashDetail*> filtered_details;
  for (const auto& match : matches) {
    // Filter out results that don't match the local full hashes.
    auto it = full_hash_to_threat_types.find(match.full_hash());
    if (it == full_hash_to_threat_types.end()) {
      continue;
    }
    const auto& relevant_sb_threat_types = it->second;
    for (const auto& detail : match.full_hash_details()) {
      // Filter out results that have threat types or attributes that are not
      // relevant to local checks in general.
      if (!IsHashDetailRelevantForLocalChecks(detail)) {
        continue;
      }
      // Only keep threat types that the client explicitly requested for this
      // full hash (based on a local list match of the prefix).
      SBThreatType result_sb_threat_type =
          v5_search_hashes_util::MapFullHashDetailToSbThreatType(detail);
      if (std::ranges::contains(relevant_sb_threat_types,
                                result_sb_threat_type)) {
        filtered_details.push_back(&detail);
      }
    }
  }

  base::UmaHistogramCounts100("SafeBrowsing.V5GetHash.ThreatInfoSize",
                              filtered_details.size());

  safe_browsing::v5_search_hashes_util::ThreatResult result =
      safe_browsing::v5_search_hashes_util::DetermineMostSevereThreat(
          filtered_details);
  return ThreatTypeAndMetadata(result.threat_type, result.metadata);
}

void V5GetHashProtocolManager::CompleteLookup(FullHashCallback callback,
                                              OperationOutcome outcome,
                                              SBThreatType threat_type,
                                              const ThreatMetadata& metadata) {
  base::UmaHistogramEnumeration("SafeBrowsing.V5GetHash.OperationOutcome",
                                outcome);
  std::move(callback).Run(threat_type, metadata);
}

}  // namespace safe_browsing
