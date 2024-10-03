// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_telemetry_uma.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"

namespace ip_protection {

namespace {

// An enumeration of the `chain_id` values supplied in the GetProxyInfo RPC
// response, for use with `UmaHistogramEnumeration`. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class ProxyChainId {
  kUnknown = 0,
  kChain1 = 1,
  kChain2 = 2,
  kChain3 = 3,
  kMaxValue = kChain3,
};

// Convert a chain_id as given in `ProxyChain::ip_protection_chain_id()` into
// a value of the `ProxyChainId` enum.
ProxyChainId ChainIdToEnum(int chain_id) {
  static_assert(net::ProxyChain::kMaxIpProtectionChainId ==
                    static_cast<int>(ProxyChainId::kMaxValue),
                "maximum `chain_id` must match between `net::ProxyChain` and "
                "`ProxyChainId`");
  CHECK(chain_id >= static_cast<int>(ProxyChainId::kUnknown) &&
        chain_id <= static_cast<int>(ProxyChainId::kMaxValue));
  return static_cast<ProxyChainId>(chain_id);
}

std::string ProxyLayerToString(ProxyLayer proxy_layer) {
  switch (proxy_layer) {
    case ProxyLayer::kProxyA:
      return "ProxyA";
    case ProxyLayer::kProxyB:
      return "ProxyB";
  }
}

}  // namespace

IpProtectionTelemetry& Telemetry() {
  static IpProtectionTelemetryUma instance;
  return instance;
}

void IpProtectionTelemetryUma::OAuthTokenFetchComplete(
    base::TimeDelta duration) {
  base::UmaHistogramTimes("NetworkService.IpProtection.OAuthTokenFetchTime",
                          duration);
}

void IpProtectionTelemetryUma::TokenBatchFetchComplete(
    TryGetAuthTokensResult result,
    std::optional<base::TimeDelta> duration) {
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.TryGetAuthTokensResult", result);
  if (duration.has_value()) {
    base::UmaHistogramTimes("NetworkService.IpProtection.TokenBatchRequestTime",
                            *duration);
  }
}

void IpProtectionTelemetryUma::AndroidTokenBatchFetchComplete(
    TryGetAuthTokensAndroidResult result,
    std::optional<base::TimeDelta> duration) {
  base::UmaHistogramEnumeration(
      "NetworkService.AwIpProtection.TryGetAuthTokensResult", result);
  if (duration.has_value()) {
    base::UmaHistogramTimes(
        "NetworkService.AwIpProtection.TokenBatchRequestTime", *duration);
  }
}

void IpProtectionTelemetryUma::ProxyChainFallback(int proxy_chain_id) {
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.ProxyChainFallback",
      ChainIdToEnum(proxy_chain_id));
}

void IpProtectionTelemetryUma::EmptyTokenCache(ProxyLayer value) {
  base::UmaHistogramEnumeration("NetworkService.IpProtection.EmptyTokenCache",
                                value);
}

void IpProtectionTelemetryUma::ProxyResolution(ProxyResolutionResult result) {
  base::UmaHistogramEnumeration("NetworkService.IpProtection.ProxyResolution",
                                result);

  // Translate the result into eligibility and availability values.
  ProtectionEligibility eligibility;
  auto record_availability = [](bool are_auth_tokens_available,
                                bool is_proxy_list_available) {
    base::UmaHistogramBoolean(
        "NetworkService.IpProtection.AreAuthTokensAvailable",
        are_auth_tokens_available);
    base::UmaHistogramBoolean(
        "NetworkService.IpProtection.IsProxyListAvailable",
        is_proxy_list_available);
    base::UmaHistogramBoolean(
        "NetworkService.IpProtection.ProtectionIsAvailableForRequest",
        are_auth_tokens_available && is_proxy_list_available);
  };

  switch (result) {
    case ProxyResolutionResult::kMdlNotPopulated:
      eligibility = ProtectionEligibility::kUnknown;
      break;
    case ProxyResolutionResult::kNoMdlMatch:
      eligibility = ProtectionEligibility::kIneligible;
      break;
    case ProxyResolutionResult::kFeatureDisabled:
      eligibility = ProtectionEligibility::kEligible;
      break;
    case ProxyResolutionResult::kSettingDisabled:
      eligibility = ProtectionEligibility::kEligible;
      break;
    case ProxyResolutionResult::kTokensNotAvailable:
      eligibility = ProtectionEligibility::kEligible;
      record_availability(
          /*are_auth_tokens_available=*/false,
          /*is_proxy_list_available=*/true);
      break;
    case ProxyResolutionResult::kProxyListNotAvailable:
      eligibility = ProtectionEligibility::kEligible;
      record_availability(
          /*are_auth_tokens_available=*/true,
          /*is_proxy_list_available=*/false);
      break;
    case ProxyResolutionResult::kTokensAndProxyListNotAvailable:
      eligibility = ProtectionEligibility::kEligible;
      record_availability(
          /*are_auth_tokens_available=*/false,
          /*is_proxy_list_available=*/false);
      break;
    case ProxyResolutionResult::kAttemptProxy:
      eligibility = ProtectionEligibility::kEligible;
      record_availability(
          /*are_auth_tokens_available=*/true,
          /*is_proxy_list_available=*/true);
      break;
    default:
      NOTREACHED();
  }

  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.RequestIsEligibleForProtection",
      eligibility);
}

void IpProtectionTelemetryUma::GetAuthTokenResultForGeo(
    bool is_token_available,
    bool enable_token_caching_by_geo,
    bool is_cache_empty,
    bool does_requested_geo_match_current) {
  base::UmaHistogramBoolean("NetworkService.IpProtection.GetAuthTokenResult",
                            is_token_available);

  // Remaining metric is only recorded when caching for geo is enabled.
  if (!enable_token_caching_by_geo) {
    return;
  }

  AuthTokenResultForGeo result;
  if (is_token_available) {
    if (does_requested_geo_match_current) {
      result = AuthTokenResultForGeo::kAvailableForCurrentGeo;
    } else {
      result = AuthTokenResultForGeo::kAvailableForOtherCachedGeo;
    }
  } else if (!is_cache_empty) {
    result = AuthTokenResultForGeo::kUnavailableButCacheContainsTokens;
  } else {
    result = AuthTokenResultForGeo::kUnavailableCacheEmpty;
  }
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.GetAuthTokenResultForGeo", result);
}

void IpProtectionTelemetryUma::TokenBatchGenerationComplete(
    base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(
      "NetworkService.IpProtection.TokenBatchGenerationTime", duration);
}

void IpProtectionTelemetryUma::GeoChangeTokenPresence(bool tokens_present) {
  base::UmaHistogramBoolean(
      "NetworkService.IpProtection.GeoChangeTokenPresence", tokens_present);
}

void IpProtectionTelemetryUma::ProxyListRefreshComplete(
    GetProxyListResult result,
    std::optional<base::TimeDelta> duration) {
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.GetProxyListResult", result);
  if (duration.has_value()) {
    base::UmaHistogramMediumTimes(
        "NetworkService.IpProtection.ProxyListRefreshTime", *duration);
  }
}

void IpProtectionTelemetryUma::TokenSpendRate(ProxyLayer proxy_layer,
                                              int value) {
  base::UmaHistogramCounts1000(
      base::StrCat({"NetworkService.IpProtection.",
                    ProxyLayerToString(proxy_layer), ".TokenSpendRate"}),
      value);
}

void IpProtectionTelemetryUma::TokenExpirationRate(ProxyLayer proxy_layer,
                                                   int value) {
  base::UmaHistogramCounts100000(
      base::StrCat({"NetworkService.IpProtection.",
                    ProxyLayerToString(proxy_layer), ".TokenExpirationRate"}),
      value);
}

void IpProtectionTelemetryUma::MdlEstimatedMemoryUsage(size_t usage) {
  // TODO(crbug.com/356109549): Consider renaming this metric.
  base::UmaHistogramMemoryKB(
      "NetworkService.MaskedDomainList.NetworkServiceProxyAllowList."
      "EstimatedMemoryUsageInKB",
      // Convert to KB
      usage / 1024);
}

void IpProtectionTelemetryUma::AndroidAuthClientCreationTime(
    base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(
      "NetworkService.IpProtection.AndroidAuthClient.CreationTime", duration);
}

void IpProtectionTelemetryUma::AndroidAuthClientGetInitialDataTime(
    base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(
      "NetworkService.IpProtection.AndroidAuthClient.GetInitialDataTime",
      duration);
}

void IpProtectionTelemetryUma::AndroidAuthClientAuthAndSignTime(
    base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(
      "NetworkService.IpProtection.AndroidAuthClient.AuthAndSignTime",
      duration);
}

void IpProtectionTelemetryUma::MdlFirstUpdateTime(base::TimeDelta duration) {
  base::UmaHistogramTimes("NetworkService.MaskedDomainList.FirstUpdateTime",
                          duration);
}

}  // namespace ip_protection
