// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_DATA_TYPES_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_DATA_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"

namespace ip_protection {

// The result of a fetch of tokens from the IP Protection auth token server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this in sync with
// IpProtectionTokenBatchRequestResult in enums.xml.
enum class TryGetAuthTokensResult {
  // The request was successful and resulted in new tokens.
  kSuccess = 0,
  // No primary account is set.
  kFailedNoAccount = 1,
  // Chrome determined the primary account is not eligible.
  kFailedNotEligible = 2,
  // There was a failure fetching an OAuth token for the primary account.
  // Deprecated in favor of `kFailedOAuthToken{Transient,Persistent}`.
  kFailedOAuthTokenDeprecated = 3,
  // There was a failure in BSA with the given status code.
  kFailedBSA400 = 4,
  kFailedBSA401 = 5,
  kFailedBSA403 = 6,

  // Any other issue calling BSA.
  kFailedBSAOther = 7,

  // There was a transient failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenTransient = 8,
  // There was a persistent failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenPersistent = 9,

  // The attempt to request tokens failed because IP Protection was disabled by
  // the user.
  kFailedDisabledByUser = 10,

  kMaxValue = kFailedDisabledByUser,
};

// The result of a fetch of tokens from the IP Protection auth token server on
// Android.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this in sync with
// AwIpProtectionTokenBatchRequestResult in enums.xml.
enum class TryGetAuthTokensAndroidResult {
  // The request was successful and resulted in new tokens.
  kSuccess = 0,
  // A transient error, implies that retrying the action (with backoff) is
  // appropriate.
  kFailedBSATransient = 1,
  // A persistent error, implies that the action should not be retried.
  kFailedBSAPersistent = 2,
  // Any other issue calling BSA.
  kFailedBSAOther = 3,
  // The attempt to request tokens failed because IP Protection is disabled by
  // WebView.
  kFailedDisabled = 4,

  kMaxValue = kFailedDisabled,
};

// A GeoHint represents a course location of a user. Values are based on
// RFC 8805 geolocation.
struct GeoHint {
  // Country code of the geo. Example: "US".
  std::string country_code;

  // ISO region of the geo. Example: "US-CA".
  std::string iso_region;

  // City name of the geo. Example: "MOUNTAIN VIEW".
  std::string city_name;

  bool operator==(const GeoHint& geo_hint) const = default;
};

// GeoId is a string representation of a GeoHint. A GeoId is
// constructed by concatenating values of the GeoHint in order of
// increasing granularity. If a finer granularity is missing, a trailing commas
// is not appended.
// Ex. GeoHint{"US", "US-CA", "MOUNTAIN VIEW"} => "US,US-CA,MOUNTAIN VIEW"
// Ex. GeoHint{"US"} => "US"
//
// Returns a formatted version of the GeoHint. In the case
// of a nullptr or empty `GeoHintPtr`, an empty string will be returned.
std::string GetGeoIdFromGeoHint(std::optional<GeoHint> geo_hint);

// Constructs a GeoHint from a GeoId string. The function
// requires a correctly formatted GeoId string. It DOES NOT handle invalid
// formats.
std::optional<GeoHint> GetGeoHintFromGeoIdForTesting(const std::string& geo_id);

// A blind-signed auth token, suitable for use with IP protection proxies.
struct BlindSignedAuthToken {
  // The token value, for inclusion in a header.
  std::string token;

  // The expiration time of this token.
  base::Time expiration;

  // The GeoHint which specifies the coarse geolocation of the token.
  GeoHint geo_hint;

  bool operator==(const BlindSignedAuthToken& token) const = default;
};

// The proxy layer to fetch batches of tokens for.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProxyLayer { kProxyA = 0, kProxyB = 1, kMaxValue = kProxyB };

// The type of MDL that is being represented. This is used to determine which
// MDL to use for a given matching request.
enum class MdlType {
  // The MDL type for IPP in incognito browsing. This is the default MDL type.
  kIncognito,

  // The MDL type for IPP experiments within regular browsing.
  kRegularBrowsing,
};

// Returns all MDL types that are represented by the given MDL resource.
//
// A given MDL resource may represent multiple MDL types. For example, a
// resource that is in the default MDL group and the regular browsing MDL group
// would return both kIncognito and kRegularBrowsing. Also, an empty vector
// indicates that the resource does not represent any MDL types which is
// unexpected and will be logged as such.
std::vector<MdlType> FromMdlResourceProto(
    const masked_domain_list::Resource& resource);

struct ProbabilisticRevealToken {
  ProbabilisticRevealToken();
  ProbabilisticRevealToken(std::int32_t version,
                           std::string u,
                           std::string e);
  ProbabilisticRevealToken(const ProbabilisticRevealToken&);
  ProbabilisticRevealToken(ProbabilisticRevealToken&&);
  ProbabilisticRevealToken& operator=(const ProbabilisticRevealToken&);
  ProbabilisticRevealToken& operator=(ProbabilisticRevealToken&&);
  ~ProbabilisticRevealToken();

  bool operator==(const ProbabilisticRevealToken& token) const = default;
  std::optional<std::string> SerializeAndEncode(std::string epoch_id) const;

  std::int32_t version = 0;
  std::string u = "";
  std::string e = "";
};

// Declares possible return status for TryGetProbabilisticRevealTokens().
// LINT.IfChange(TryGetProbabilisticRevealTokensStatus)
enum class TryGetProbabilisticRevealTokensStatus {
  kSuccess = 0,
  kNetNotOk = 1,
  kNetOkNullResponse = 2,
  kNullResponse = 3,
  kResponseParsingFailed = 4,
  kInvalidTokenVersion = 5,
  kInvalidTokenSize = 6,
  kTooFewTokens = 7,
  kTooManyTokens = 8,
  kExpirationTooSoon = 9,
  kExpirationTooLate = 10,
  kInvalidPublicKey = 11,
  kInvalidNumTokensWithSignal = 12,
  kRequestBackedOff = 13,
  kNoGoogleChromeBranding = 14,
  kInvalidEpochIdSize = 15,
  kMaxValue = kInvalidEpochIdSize,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:ProbabilisticRevealTokensResult)

// Stores return status of TryGetProbabilisticRevealTokens() together with
// NetError() returned by url loader.
struct TryGetProbabilisticRevealTokensResult {
  // Stores return status of TryGetProbabilisticRevealTokens().
  TryGetProbabilisticRevealTokensStatus status;
  // Stores url_loader->NetError() after calling url_loader->DownloadToString()
  // in Retriever::RetrieveProbabilisticRevealTokens(). `network_error_code` is
  // not net::OK if `result` is kNetNotOk. `network_error_code` is net::OK for
  // all other `result` values. `network_error_code` is net::OK if
  // url_loader->DownloadToString() is not called yet, for example when
  // `status` is kProxyDisabled and TryGetProbabilisticRevealTokens returned
  // before making a network call.
  int network_error_code;
  // Stores the time when the next TryGetProbabilisticRevealTokens() call should
  // be made. `try_again_after` is set on network errors (i.e. when `status` is
  // kNetNotOk or kNetOkNullResponse), nullopt otherwise.
  std::optional<base::Time> try_again_after;
};

// Stores parsed TryGetProbabilisticRevealTokensResponse for successfully parsed
// responses.
struct TryGetProbabilisticRevealTokensOutcome {
  TryGetProbabilisticRevealTokensOutcome();
  TryGetProbabilisticRevealTokensOutcome(
      const TryGetProbabilisticRevealTokensOutcome&);
  TryGetProbabilisticRevealTokensOutcome(
      TryGetProbabilisticRevealTokensOutcome&&);
  TryGetProbabilisticRevealTokensOutcome& operator=(
      const TryGetProbabilisticRevealTokensOutcome&);
  TryGetProbabilisticRevealTokensOutcome& operator=(
      TryGetProbabilisticRevealTokensOutcome&&);
  ~TryGetProbabilisticRevealTokensOutcome();

  std::vector<ProbabilisticRevealToken> tokens;
  std::string public_key;
  std::uint64_t expiration_time_seconds;
  std::uint64_t next_epoch_start_time_seconds;
  std::int32_t num_tokens_with_signal;
  std::string epoch_id;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_DATA_TYPES_H_
