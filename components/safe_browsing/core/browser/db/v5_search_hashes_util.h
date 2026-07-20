// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_SEARCH_HASHES_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_SEARCH_HASHES_UTIL_H_

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"

namespace safe_browsing {

class V5SearchHashesCache;

namespace v5_search_hashes_util {

// The baseline severity value representing the least severe threat.
inline constexpr int kLeastSeverity = std::numeric_limits<int>::max();

// Failures encountered when parsing and validating the V5 SearchHashes
// response.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(V5GetHashParseFailureReason)
enum class ParseFailure {
  // A connection-level network error occurred.
  kNetworkError = 1,
  // An HTTP error status code was returned.
  kHttpError = 2,
  // A transient error occurred that doesn't need to affect backoff.
  kRetriableError = 3,
  // Failed to parse the response body as a protobuf.
  kParseError = 4,
  // The response is missing the required cache duration field.
  kNoCacheDurationError = 5,
  // One or more full hashes in the response have an incorrect length.
  kIncorrectFullHashLengthError = 6,
  kMaxValue = kIncorrectFullHashLengthError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV5GetHashParseFailureReason)

// Holds the successfully parsed and sanitized V5 SearchHashes response.
struct ParseResultSuccess {
  ParseResultSuccess(V5::SearchHashesResponse response,
                     bool found_unmatched_full_hashes);
  ~ParseResultSuccess();
  ParseResultSuccess(ParseResultSuccess&&);
  ParseResultSuccess& operator=(ParseResultSuccess&&);

  // The parsed and sanitized response proto.
  V5::SearchHashesResponse response;
  // True if the response contained full hashes that didn't match the requested
  // prefixes. (Note: they have been removed.)
  bool found_unmatched_full_hashes = false;
};

// Result of determining the most severe threat.
struct ThreatResult {
  ThreatResult();
  ~ThreatResult();
  ThreatResult(const ThreatResult&);
  ThreatResult& operator=(const ThreatResult&);

  bool operator==(const ThreatResult&) const = default;

  // The determined threat type.
  SBThreatType threat_type = SBThreatType::SB_THREAT_TYPE_SAFE;
  // Metadata associated with the threat.
  ThreatMetadata metadata;
  // The severity level of the threat (lower is more severe).
  int threat_severity = kLeastSeverity;
};

// Maps the V5 protobuf ThreatType in the `detail` to an SBThreatType.
SBThreatType MapFullHashDetailToSbThreatType(
    const V5::FullHash::FullHashDetail& detail);

// Determines the most severe threat among the provided full hash details.
// - details: These must already be filtered down to the ones the client
//   considers a valid match on their requested hash prefixes (e.g. matches the
//   corresponding full hashes, matches the expected threat types).
// Returns the ThreatResult representing the most severe threat.
ThreatResult DetermineMostSevereThreat(
    const std::vector<const V5::FullHash::FullHashDetail*>& filtered_details);

// Base64-encodes the request and generates the search hashes URL.
//  - `request`: The request proto to encode.
std::string GetResourceUrl(V5::SearchHashesRequest* request);

// Searches the local cache for the input `unique_prefixes`.
//  - `cache`: The cache to search.
//  - `unique_prefixes`: The set of prefixes to look up.
//  - `out_hash_prefixes_to_request`: Output parameter with a list of which
//    hash prefixes were not found in the cache and need to be requested.
//  - `out_cached_full_hashes`: Output parameter with a list of unsafe
//    full hashes that were found in the cache for any of the `unique_prefixes`.
void SearchCache(V5SearchHashesCache* cache,
                 const std::set<std::string>& unique_prefixes,
                 std::vector<std::string>* out_hash_prefixes_to_request,
                 std::vector<V5::FullHash>* out_cached_full_hashes);

// Evaluates network/HTTP errors, parses the response body, and sanitizes the
// results.
// - net_error: The network error code returned by the loader.
// - response_code: The HTTP response code returned by the server.
// - response_body: The raw serialized response body.
// - requested_hash_prefixes: The list of hash prefixes that were requested.
//   Used to filter out full hashes that do not match these prefixes.
// Returns a ParseResultSuccess struct on success, or a ParseFailure enum
// representing the failure.
base::expected<ParseResultSuccess, ParseFailure> ParseResponse(
    int net_error,
    int response_code,
    const std::string& response_body,
    const std::vector<std::string>& requested_hash_prefixes);

}  // namespace v5_search_hashes_util

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_SEARCH_HASHES_UTIL_H_
