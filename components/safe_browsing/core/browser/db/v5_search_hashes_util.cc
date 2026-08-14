// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_search_hashes_util.h"

#include <algorithm>
#include <utility>

#include "base/base64url.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "components/safe_browsing/core/common/utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
namespace safe_browsing::v5_search_hashes_util {

namespace {

std::optional<ParseFailure> EvaluateNetworkResult(int net_error,
                                                  int response_code) {
  if (net_error != net::OK &&
      net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    return ErrorIsRetriable(net_error, response_code)
               ? ParseFailure::kRetriableError
               : ParseFailure::kNetworkError;
  }
  if (response_code != net::HTTP_OK) {
    return ParseFailure::kHttpError;
  }
  return std::nullopt;
}

void AddToSubresourceFilterMetadata(
    ThreatMetadata* subresource_filter_metadata,
    const V5::FullHash::FullHashDetail& detail) {
  SubresourceFilterLevel level =
      std::ranges::contains(detail.attributes(), V5::ThreatAttribute::CANARY)
          ? SubresourceFilterLevel::WARN
          : SubresourceFilterLevel::ENFORCE;
  if (detail.threat_type() == V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION) {
    auto it = subresource_filter_metadata->subresource_filter_match.find(
        SubresourceFilterType::ABUSIVE);
    if (it == subresource_filter_metadata->subresource_filter_match.end() ||
        level > it->second) {
      subresource_filter_metadata
          ->subresource_filter_match[SubresourceFilterType::ABUSIVE] = level;
    }
  } else if (detail.threat_type() == V5::ThreatType::BETTER_ADS_VIOLATION) {
    auto it = subresource_filter_metadata->subresource_filter_match.find(
        SubresourceFilterType::BETTER_ADS);
    if (it == subresource_filter_metadata->subresource_filter_match.end() ||
        level > it->second) {
      subresource_filter_metadata
          ->subresource_filter_match[SubresourceFilterType::BETTER_ADS] = level;
    }
  }
}

int GetThreatSeverity(const V5::FullHash::FullHashDetail& detail) {
  bool is_canary =
      std::ranges::contains(detail.attributes(), V5::ThreatAttribute::CANARY);
  if (detail.threat_type() == V5::ThreatType::SOCIAL_ENGINEERING && is_canary) {
    // SUSPICIOUS threat type.
    return 7;
  }
  // LINT.IfChange(ThreatTypeSeverity)
  switch (detail.threat_type()) {
    case V5::ThreatType::MALWARE:
    case V5::ThreatType::SOCIAL_ENGINEERING:
    case V5::ThreatType::MALICIOUS_BINARY:
      return 0;
    case V5::ThreatType::UNWANTED_SOFTWARE:
      return 1;
    // Subresource filter threat types are ranked according to the hierarchy:
    // BETTER_ADS > ABUSIVE > BETTER_ADS (CANARY) > ABUSIVE (CANARY)
    case V5::ThreatType::BETTER_ADS_VIOLATION:
      return is_canary ? 4 : 2;
    case V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION:
      return is_canary ? 5 : 3;
    case V5::ThreatType::NOTIFICATION_ABUSE:
      return 2;
    case V5::ThreatType::TRICK_TO_BILL:
      return 15;
    case V5::ThreatType::SUBRESOURCE_FILTER:
    case V5::ThreatType::THREAT_TYPE_UNSPECIFIED:
    case V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION:
      NOTREACHED();
      // LINT.ThenChange(//components/safe_browsing/core/common/proto/safebrowsingv5.proto:ThreatType)
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      NOTREACHED();
  }
}

bool RemoveUnmatchedFullHashes(
    V5::SearchHashesResponse* response,
    const std::vector<std::string>& requested_hash_prefixes) {
  size_t initial_full_hashes_count = response->full_hashes_size();
  std::set<std::string> requested_hash_prefixes_set(
      requested_hash_prefixes.begin(), requested_hash_prefixes.end());
  auto* mutable_full_hashes = response->mutable_full_hashes();
  // Removes full hashes from response if their prefixes don't match the
  // client's requested hash prefixes. This is a server bug if it occurs (and
  // does actually happen in practice).
  mutable_full_hashes->erase(
      std::remove_if(
          mutable_full_hashes->begin(), mutable_full_hashes->end(),
          [requested_hash_prefixes_set](const V5::FullHash& full_hash) {
            return !requested_hash_prefixes_set.contains(
                SBProtocolManagerUtil::GetHashPrefix(full_hash.full_hash()));
          }),
      mutable_full_hashes->end());
  size_t final_full_hashes_count = response->full_hashes_size();
  return initial_full_hashes_count != final_full_hashes_count;
}

void RemoveFullHashDetailsWithInvalidEnums(V5::SearchHashesResponse* response) {
  for (int i = 0; i < response->full_hashes_size(); ++i) {
    auto* mutable_details =
        response->mutable_full_hashes(i)->mutable_full_hash_details();
    // Remove results with newer (or invalid) threat types / attributes that
    // the client's current version is not aware of.
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

}  // namespace

ThreatResult::ThreatResult() = default;
ThreatResult::~ThreatResult() = default;
ThreatResult::ThreatResult(const ThreatResult&) = default;
ThreatResult& ThreatResult::operator=(const ThreatResult&) = default;

SBThreatType MapFullHashDetailToSbThreatType(
    const V5::FullHash::FullHashDetail& detail) {
  if (detail.threat_type() == V5::ThreatType::SOCIAL_ENGINEERING &&
      std::ranges::contains(detail.attributes(), V5::ThreatAttribute::CANARY)) {
    return SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE;
  }
  // LINT.IfChange(ThreatTypeMap)
  switch (detail.threat_type()) {
    case V5::ThreatType::MALWARE:
      return SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
    case V5::ThreatType::SOCIAL_ENGINEERING:
      return SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    case V5::ThreatType::UNWANTED_SOFTWARE:
      return SBThreatType::SB_THREAT_TYPE_URL_UNWANTED;
    case V5::ThreatType::TRICK_TO_BILL:
      return SBThreatType::SB_THREAT_TYPE_BILLING;
    case V5::ThreatType::MALICIOUS_BINARY:
      return SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE;
    case V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION:
    case V5::ThreatType::BETTER_ADS_VIOLATION:
      return SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER;
    case V5::ThreatType::NOTIFICATION_ABUSE:
      return SBThreatType::SB_THREAT_TYPE_API_ABUSE;
    case V5::ThreatType::THREAT_TYPE_UNSPECIFIED:
    case V5::ThreatType::SUBRESOURCE_FILTER:
    case V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION:
      NOTREACHED();
      // LINT.ThenChange(//components/safe_browsing/core/common/proto/safebrowsingv5.proto:ThreatType)
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      NOTREACHED();
  }
}

ThreatResult DetermineMostSevereThreat(
    const std::vector<const V5::FullHash::FullHashDetail*>& filtered_details) {
  ThreatResult result;
  ThreatMetadata subresource_filter_metadata;
  for (const auto* detail_ptr : filtered_details) {
    const auto& detail = *detail_ptr;
    int severity = GetThreatSeverity(detail);
    if (severity < result.threat_severity) {
      result.threat_severity = severity;
      result.threat_type = MapFullHashDetailToSbThreatType(detail);
    }
    AddToSubresourceFilterMetadata(&subresource_filter_metadata, detail);
  }
  if (result.threat_type == SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
    result.metadata = std::move(subresource_filter_metadata);
  }
  return result;
}

std::string GetResourceUrl(V5::SearchHashesRequest* request) {
  std::string request_data, request_base64;
  request->SerializeToString(&request_data);
  base::Base64UrlEncode(request_data,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &request_base64);

  std::string url = base::StringPrintf(
      "https://safebrowsing.googleapis.com/v5/hashes:search"
      "?$req=%s&$ct=application/x-protobuf",
      request_base64.c_str());
  auto api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(
        &url, "&key=%s",
        base::EscapeQueryParamValue(api_key, /*use_plus=*/true).c_str());
  }
  return url;
}

void SearchCache(V5SearchHashesCache* cache,
                 const std::set<std::string>& unique_prefixes,
                 std::vector<std::string>* out_hash_prefixes_to_request,
                 std::vector<V5::FullHash>* out_cached_full_hashes) {
  auto cached_results =
      cache ? cache->SearchCache(unique_prefixes)
            : std::unordered_map<std::string, std::vector<V5::FullHash>>();
  for (const auto& prefix : unique_prefixes) {
    auto it = cached_results.find(prefix);
    if (it != cached_results.end()) {
      // If in the cache, keep track of associated full hashes to merge them
      // with the response results later.
      out_cached_full_hashes->insert(out_cached_full_hashes->end(),
                                     it->second.begin(), it->second.end());
    } else {
      // If not in the cache, add the prefix to hash prefixes to request.
      out_hash_prefixes_to_request->push_back(prefix);
    }
  }
}

ParseResultSuccess::ParseResultSuccess(V5::SearchHashesResponse response,
                                       bool found_unmatched_full_hashes)
    : response(std::move(response)),
      found_unmatched_full_hashes(found_unmatched_full_hashes) {}
ParseResultSuccess::~ParseResultSuccess() = default;
ParseResultSuccess::ParseResultSuccess(ParseResultSuccess&&) = default;
ParseResultSuccess& ParseResultSuccess::operator=(ParseResultSuccess&&) =
    default;

base::expected<ParseResultSuccess, ParseFailure> ParseResponse(
    int net_error,
    int response_code,
    const std::string& response_body,
    const std::vector<std::string>& requested_hash_prefixes) {
  std::optional<ParseFailure> net_result =
      EvaluateNetworkResult(net_error, response_code);
  if (net_result.has_value()) {
    return base::unexpected(net_result.value());
  }
  V5::SearchHashesResponse response;
  if (!response.ParseFromString(response_body)) {
    return base::unexpected(ParseFailure::kParseError);
  }
  if (!response.has_cache_duration()) {
    return base::unexpected(ParseFailure::kNoCacheDurationError);
  }
  for (const auto& full_hash : response.full_hashes()) {
    if (full_hash.full_hash().length() != 32) {
      return base::unexpected(ParseFailure::kIncorrectFullHashLengthError);
    }
  }
  bool found_unmatched_full_hashes =
      RemoveUnmatchedFullHashes(&response, requested_hash_prefixes);
  RemoveFullHashDetailsWithInvalidEnums(&response);
  return ParseResultSuccess(std::move(response), found_unmatched_full_hashes);
}

}  // namespace safe_browsing::v5_search_hashes_util
