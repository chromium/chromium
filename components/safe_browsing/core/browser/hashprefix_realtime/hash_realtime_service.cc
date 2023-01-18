// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"

#include "base/base64url.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

const size_t kLookupTimeoutDurationInSeconds = 3;

SBThreatType MapThreatTypeToSbThreatType(const V5::ThreatType& threat_type) {
  switch (threat_type) {
    case V5::ThreatType::MALWARE:
      return SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
    case V5::ThreatType::SOCIAL_ENGINEERING:
      return SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    case V5::ThreatType::UNWANTED_SOFTWARE:
      return SBThreatType::SB_THREAT_TYPE_URL_UNWANTED;
    case V5::ThreatType::SUSPICIOUS:
      return SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE;
    case V5::ThreatType::TRICK_TO_BILL:
      return SBThreatType::SB_THREAT_TYPE_BILLING;
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      NOTREACHED() << "Unexpected ThreatType encountered: " << threat_type;
      return SBThreatType::SB_THREAT_TYPE_UNUSED;
  }
}

}  // namespace

HashRealTimeService::HashRealTimeService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

HashRealTimeService::~HashRealTimeService() = default;

// static
bool HashRealTimeService::CanCheckUrl(
    const GURL& url,
    network::mojom::RequestDestination request_destination) {
  return request_destination == network::mojom::RequestDestination::kDocument &&
         CanGetReputationOfUrl(url);
}
SBThreatType HashRealTimeService::DetermineSBThreatType(
    const GURL& url,
    const std::vector<V5::FullHash>& result_full_hashes) {
  std::vector<std::string> url_full_hashes_vector;
  V4ProtocolManagerUtil::UrlToFullHashes(url, &url_full_hashes_vector);
  std::set<std::string> url_full_hashes(url_full_hashes_vector.begin(),
                                        url_full_hashes_vector.end());
  SBThreatType sb_threat_type = SBThreatType::SB_THREAT_TYPE_SAFE;
  int threat_severity = kLeastSeverity;
  for (const auto& hash_proto : result_full_hashes) {
    auto it = url_full_hashes.find(hash_proto.full_hash());
    if (url_full_hashes.end() != it) {
      for (const auto& detail : hash_proto.full_hash_details()) {
        if (hash_realtime_utils::IsThreatTypeRelevant(detail.threat_type())) {
          // Note that for hash-prefix real-time checks, there is no need to use
          // the attributes field, because all the checks are for frame URLs.
          if (IsThreatTypeMoreSevere(detail.threat_type(), threat_severity)) {
            threat_severity = GetThreatSeverity(detail.threat_type());
            sb_threat_type = MapThreatTypeToSbThreatType(detail.threat_type());
          }
        }
      }
    }
  }
  return sb_threat_type;
}
int HashRealTimeService::GetThreatSeverity(const V5::ThreatType& threat_type) {
  // These values should be consistent with the ones in GetThreatSeverity in
  // v4_local_database_manager.cc.
  switch (threat_type) {
    case V5::ThreatType::MALWARE:
    case V5::ThreatType::SOCIAL_ENGINEERING:
      return 0;
    case V5::ThreatType::UNWANTED_SOFTWARE:
      return 1;
    case V5::ThreatType::SUSPICIOUS:
      return 4;
    case V5::ThreatType::TRICK_TO_BILL:
      return 15;
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      NOTREACHED() << "Unexpected ThreatType encountered: " << threat_type;
      return kLeastSeverity;
  }
}
bool HashRealTimeService::IsThreatTypeMoreSevere(
    const V5::ThreatType& threat_type,
    int baseline_severity) {
  auto candidate_severity = GetThreatSeverity(threat_type);
  return candidate_severity < baseline_severity;
}

void HashRealTimeService::StartLookup(
    const GURL& url,
    HPRTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());

  // Prepare request.
  auto request = std::make_unique<V5::SearchHashesRequest>();
  std::vector<std::string> full_hashes;
  V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  for (const auto& full_hash : full_hashes) {
    auto hash_prefix = hash_realtime_utils::GetHashPrefix(full_hash);
    request->add_hash_prefixes(hash_prefix);
  }

  // Send request.
  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(GetResourceRequest(std::move(request)),
                                       GetTrafficAnnotationTag());
  owned_loader->SetTimeoutDuration(
      base::Seconds(kLookupTimeoutDurationInSeconds));
  owned_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&HashRealTimeService::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), url, owned_loader.get(),
                     std::move(callback_task_runner)));
  pending_requests_[owned_loader.release()] = std::move(response_callback);
}

void HashRealTimeService::OnURLLoaderComplete(
    const GURL& url,
    network::SimpleURLLoader* url_loader,
    scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto pending_request_it = pending_requests_.find(url_loader);
  DCHECK(pending_request_it != pending_requests_.end()) << "Request not found";

  int net_error = url_loader->NetError();
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  auto response =
      ParseResponse(net_error, response_code, std::move(response_body));
  absl::optional<SBThreatType> sb_threat_type;
  if (response.has_value()) {
    sb_threat_type =
        DetermineSBThreatType(url, {response.value()->full_hashes().begin(),
                                    response.value()->full_hashes().end()});
  }

  response_callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(pending_request_it->second),
                                /*is_lookup_successful=*/response.has_value(),
                                sb_threat_type));

  delete pending_request_it->first;
  pending_requests_.erase(pending_request_it);
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

absl::optional<std::unique_ptr<V5::SearchHashesResponse>>
HashRealTimeService::ParseResponse(
    int net_error,
    int response_code,
    std::unique_ptr<std::string> response_body) const {
  auto response = std::make_unique<V5::SearchHashesResponse>();
  bool net_and_http_ok = net_error == net::OK && response_code == net::HTTP_OK;
  if (net_and_http_ok && response->ParseFromString(*response_body)) {
    if (!response->has_cache_duration()) {
      return absl::nullopt;
    }
    for (const auto& full_hash : response->full_hashes()) {
      if (full_hash.full_hash().length() !=
          hash_realtime_utils::kFullHashLength) {
        return absl::nullopt;
      }
    }
    RemoveFullHashDetailsWithInvalidEnums(response);
    return std::move(response);
  }
  return absl::nullopt;
}

std::unique_ptr<network::ResourceRequest>
HashRealTimeService::GetResourceRequest(
    std::unique_ptr<V5::SearchHashesRequest> request) const {
  std::string request_data, request_base64;
  request->SerializeToString(&request_data);
  base::Base64UrlEncode(request_data,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &request_base64);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string url = base::StringPrintf(
      "https://safebrowsing.googleapis.com/v5alpha1/hashes:search"
      "?$req=%s&$ct=application/x-protobuf",
      request_base64.c_str());
  auto api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        base::EscapeQueryParamValue(api_key, true).c_str());
  }
  resource_request->url = GURL(url);
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return resource_request;
}

void HashRealTimeService::Shutdown() {
  for (auto& pending : pending_requests_) {
    // Pending requests are not posted back to the IO thread during shutdown,
    // because it is too late to post a task to the IO thread when the UI
    // thread is shutting down.
    delete pending.first;
  }
  pending_requests_.clear();
}

net::NetworkTrafficAnnotationTag HashRealTimeService::GetTrafficAnnotationTag()
    const {
  return net::DefineNetworkTrafficAnnotation(
      "safe_browsing_hashprefix_realtime_lookup",
      R"(
  semantics {
    sender: "Safe Browsing"
    description:
      "When Safe Browsing can't detect that a URL is safe based on its "
      "local database, it sends partial hashes of the URL to Google to check "
      "whether to show a warning to the user. These partial hashes do not "
      "expose the URL to Google."
    trigger:
      "When a main frame URL fails to match the local hash-prefix "
      "database of known safe URLs and a valid result from a prior "
      "lookup is not already cached, this will be sent."
    data:
        "The 32-bit hash prefixes of the URL that did not match the local "
        " safelist. The URL itself is not sent."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts {
        email: "thefrog@chromium.org"
      }
    }
    user_data {
      type: NONE
    }
    last_reviewed: "2023-01-18"
  }
  policy {
    cookies_allowed: YES
    cookies_store: "Safe Browsing cookie store"
    setting:
      "Users can disable Safe Browsing by unchecking 'Protect you and "
      "your device from dangerous sites' in Chromium settings under "
      "Privacy. The feature is enabled by default."
    chrome_policy {
      SafeBrowsingEnabled {
        policy_options {mode: MANDATORY}
        SafeBrowsingEnabled: false
      }
    }
  })");
}

}  // namespace safe_browsing
