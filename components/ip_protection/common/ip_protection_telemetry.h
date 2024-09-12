// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_H_

#include <optional>

#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "net/base/proxy_chain.h"

namespace ip_protection {

// An enumeration of the eligibility finding for use with
// `UmaHistogramEnumeration`. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
enum class ProtectionEligibility {
  kUnknown = 0,
  kIneligible = 1,
  kEligible = 2,
  kMaxValue = kEligible,
};

// An enumeration of the result of an attempt to fetch a proxy list. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class GetProxyListResult {
  kFailed = 0,
  kEmptyList = 1,
  kPopulatedList = 2,
  kMaxValue = kPopulatedList,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AuthTokenResultForGeo)
enum class AuthTokenResultForGeo {
  kUnavailableCacheEmpty = 0,
  kUnavailableButCacheContainsTokens = 1,
  kAvailableForCurrentGeo = 2,
  kAvailableForOtherCachedGeo = 3,
  kMaxValue = kAvailableForOtherCachedGeo,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:IpProtectionGetAuthTokenResultForGeo)

// An abstract interface for all of the telemetry associated with IP Protection.
//
// This is implemented by each telemetry platform, and a singleton made
// available on a per-process basis.
//
// More detail on the metrics produced here can be found in
// `tools/metrics/histograms/metadata/network/histograms.xml`.
class IpProtectionTelemetry {
 public:
  virtual ~IpProtectionTelemetry() = default;

  // Methods to make measurements of specific events related to IP Protection.

  // An OAuth token was fetched successfully, for purposes of authenticating
  // calls to getInitialData and authAndSign (not for getProxyConfig).
  virtual void OAuthTokenFetchComplete(base::TimeDelta duration) = 0;

  // A token batch was fetched. If result is not `kSuccess`, then the duration
  // is nullopt. Records the elapsed time for successful requests by
  // `IpProtectionConfigGetter` for blind-signed tokens from BSA.
  virtual void TokenBatchFetchComplete(
      TryGetAuthTokensResult result,
      std::optional<base::TimeDelta> duration) = 0;

  // Completed an attempt to fetch tokens via the system-provided auth service
  // on Android.
  virtual void AndroidTokenBatchFetchComplete(
      TryGetAuthTokensAndroidResult result,
      std::optional<base::TimeDelta> duration) = 0;

  // Chrome has determined that a proxy chain with the given chain ID has failed
  // and fallen back to the next chain in the list.
  virtual void ProxyChainFallback(int proxy_chain_id) = 0;

  // The token cache for the given layer was empty during a call to
  // OnResolveProxy.
  virtual void EmptyTokenCache(ProxyLayer) = 0;

  // An eligibility determination has been made for a request, in a call to
  // `OnResolveProxy`.
  virtual void RequestIsEligibleForProtection(ProtectionEligibility) = 0;

  // An availability determination has been made for a request, in a call to
  // `OnResolveProxy`. This only occurs when the request is eligible and IP
  // Protection is enabled. Protection is considered available if both tokens
  // and a proxy list are available.
  virtual void ProtectionIsAvailableForRequest(
      bool are_auth_tokens_available,
      bool is_proxy_list_available) = 0;

  // Results of a call to GetAuthToken. `is_token_available` is true if a token
  // was returned; `enable_token_caching_by_geo` represents the feature status;
  // `is_cache_empty` is true if the manager has no cached tokens (for any geo);
  // and `does_requested_geo_match_current` is true if the token request was
  // made for the current geo.
  virtual void GetAuthTokenResultForGeo(
      bool is_token_available,
      bool enable_token_caching_by_geo,
      bool is_cache_empty,
      bool does_requested_geo_match_current) = 0;

  // Token batch generation has completed, with the given duration.
  // This measures the whole token batch generation process, from an
  // `IpProtectionTokenManagerImpl`'s perspective, from just before calling
  // `IpProtectionConfigGetter::TryGetAuthTokens` until `OnGotAuthTokens`.
  virtual void TokenBatchGenerationComplete(base::TimeDelta duration) = 0;

  // Whether tokens already exist for a new geo, as measured when current geo
  // changes.
  virtual void GeoChangeTokenPresence(bool) = 0;

  // A refresh of the proxy list has completed. Duration is set unless the
  // result is `kFailed`.
  virtual void ProxyListRefreshComplete(
      GetProxyListResult result,
      std::optional<base::TimeDelta> duration) = 0;

  // Token spend rate, in tokens per hour. This value is expected to be less
  // than 1000.
  virtual void TokenSpendRate(ProxyLayer, int) = 0;

  // Token expiration rate, in tokens per hour. This value is expected to be
  // less than 100,000.
  virtual void TokenExpirationRate(ProxyLayer, int) = 0;

  // The estimated memory usage of the MDL, in KB. This is emitted after the
  // MDL is fully loaded/updated (with any exclusions applied).
  virtual void MdlEstimatedMemoryUsage(size_t) = 0;

  // Time taken to create an Android IP Protection auth client, including
  // binding to the system-provided auth service.
  virtual void AndroidAuthClientCreationTime(base::TimeDelta duration) = 0;

  // Time taken to perform a successful GetInitialData request via
  // the Android auth client/service.
  virtual void AndroidAuthClientGetInitialDataTime(
      base::TimeDelta duration) = 0;

  // Time taken to perform a successful AuthAndSign request via
  // the Android auth client/service.
  virtual void AndroidAuthClientAuthAndSignTime(base::TimeDelta duration) = 0;

  // Delay between the MDL manager being created and UpdateMaskedDomainList
  // first being called.
  virtual void MdlFirstUpdateTime(base::TimeDelta duration) = 0;
};

// Get the singleton instance of this type. This will be implemented by each
// subclass, with only one being built on any particular platform.
IpProtectionTelemetry& Telemetry();

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_H_
