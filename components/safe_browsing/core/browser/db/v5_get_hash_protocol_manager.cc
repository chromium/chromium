// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_util.h"
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

bool IsHashDetailRelevantForLocalChecks(
    const V5::FullHash::FullHashDetail& detail) {
  bool has_canary = false;
  bool has_frame_only = false;

  for (int attribute_value : detail.attributes()) {
    switch (attribute_value) {
      case V5::ThreatAttribute::CANARY:
        has_canary = true;
        break;
      case V5::ThreatAttribute::FRAME_ONLY:
        has_frame_only = true;
        break;
      case V5::ThreatAttribute::THREAT_ATTRIBUTE_UNSPECIFIED:
        break;
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

  // CANARY should only be attached with SOCIAL_ENGINEERING.
  switch (detail.threat_type()) {
    case V5::ThreatType::SOCIAL_ENGINEERING:
      // These types support both warning (CANARY) and enforcement.
      return true;

    case V5::ThreatType::MALWARE:
    case V5::ThreatType::UNWANTED_SOFTWARE:
    case V5::ThreatType::TRICK_TO_BILL:
      // These types only support enforcement; CANARY is invalid.
      return !has_canary;

    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      return false;
  }
}

}  // namespace

V5GetHashProtocolManager::V5GetHashProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config)
    : url_loader_factory_(url_loader_factory), config_(config) {}

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

  // Determine which hash prefixes we are interested in.
  std::set<std::string> unique_prefixes;
  for (const auto& entry : full_hash_to_threat_types) {
    unique_prefixes.insert(SBProtocolManagerUtil::GetHashPrefix(entry.first));
  }
  std::vector<std::string> hash_prefixes_to_request(unique_prefixes.begin(),
                                                    unique_prefixes.end());

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
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&V5GetHashProtocolManager::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), loader,
                     full_hash_to_threat_types,
                     std::move(hash_prefixes_to_request), std::move(callback)));

  pending_loaders_.insert(std::move(owned_loader));
}

void V5GetHashProtocolManager::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types,
    std::vector<std::string> requested_prefixes,
    FullHashCallback callback,
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<std::unique_ptr<network::SimpleURLLoader>,
           base::UniquePtrComparator>::iterator it =
      pending_loaders_.find(url_loader);
  CHECK(it != pending_loaders_.end()) << "Request not found";
  std::unique_ptr<network::SimpleURLLoader> loader_to_delete =
      std::move(pending_loaders_.extract(it).value());

  // Parse the result.
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }
  int net_error = url_loader->NetError();
  base::expected<v5_search_hashes_util::ParseResultSuccess,
                 v5_search_hashes_util::ParseFailure>
      parse_info = safe_browsing::v5_search_hashes_util::ParseResponse(
          net_error, response_code, response_body.value_or(""),
          requested_prefixes);

  // Return upon error.
  if (!parse_info.has_value()) {
    std::move(callback).Run(SBThreatType::SB_THREAT_TYPE_SAFE,
                            ThreatMetadata());
    return;
  }

  const V5::SearchHashesResponse& response_proto = parse_info->response;
  std::vector<V5::FullHash> response_full_hashes(
      response_proto.full_hashes().begin(), response_proto.full_hashes().end());

  ThreatTypeAndMetadata result = DetermineMostSevereThreatForLocalChecks(
      response_full_hashes, full_hash_to_threat_types);
  std::move(callback).Run(result.threat_type, result.metadata);
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

  safe_browsing::v5_search_hashes_util::ThreatResult result =
      safe_browsing::v5_search_hashes_util::DetermineMostSevereThreat(
          filtered_details);
  return ThreatTypeAndMetadata(result.threat_type, ThreatMetadata());
}

}  // namespace safe_browsing
