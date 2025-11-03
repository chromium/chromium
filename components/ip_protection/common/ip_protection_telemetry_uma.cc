// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_telemetry_uma.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "net/base/proxy_chain.h"

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

// Converts an IpProtectionTokenCountEvent enum value to its corresponding
// string representation for histogram naming.
std::string TokenCountEventToString(IpProtectionTokenCountEvent event) {
  switch (event) {
    case IpProtectionTokenCountEvent::kIssued:
      return "Issued";
    case IpProtectionTokenCountEvent::kSpent:
      return "Spent";
    case IpProtectionTokenCountEvent::kExpired:
      return "Expired";
    case IpProtectionTokenCountEvent::kOrphaned:
      return "Orphaned";
    case IpProtectionTokenCountEvent::kRecycled:
      return "Recycled";
  }
  NOTREACHED();
}

// Converts a BlindSignAuthPhase enum value to its corresponding string
// representation for histogram naming.
std::string_view BlindSignAuthPhaseToString(BlindSignAuthPhase phase) {
  switch (phase) {
    case BlindSignAuthPhase::kGetInitialData:
      return "GetInitialData";
    case BlindSignAuthPhase::kGenerateBlindedTokenRequests:
      return "GenerateBlindedTokenRequests";
    case BlindSignAuthPhase::kAuthAndSign:
      return "AuthAndSign";
    case BlindSignAuthPhase::kUnblindTokens:
      return "UnblindTokens";
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
      "NetworkService.IpProtection.TryGetAuthTokensResult2", result);
  if (duration.has_value()) {
    base::UmaHistogramTimes("NetworkService.IpProtection.TokenBatchRequestTime",
                            *duration);
  }
}

void IpProtectionTelemetryUma::ProxyChainFallback(int proxy_chain_id) {
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.ProxyChainFallback",
      ChainIdToEnum(proxy_chain_id));
}

void IpProtectionTelemetryUma::EmptyTokenCache(ProxyLayer value) {
  base::UmaHistogramEnumeration("NetworkService.IpProtection.EmptyTokenCache2",
                                value);
}

void IpProtectionTelemetryUma::ProxyResolution(ProxyResolutionResult result) {
  base::UmaHistogramEnumeration("NetworkService.IpProtection.ProxyResolution",
                                result);
}

void IpProtectionTelemetryUma::GetAuthTokenResultForGeo(
    bool is_token_available,
    bool is_cache_empty,
    bool does_requested_geo_match_current) {
  base::UmaHistogramBoolean("NetworkService.IpProtection.GetAuthTokenResult",
                            is_token_available);
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

void IpProtectionTelemetryUma::TokenBatchGenerationPhaseTime(
    BlindSignAuthPhase phase,
    base::TimeDelta duration) {
  base::UmaHistogramTimes(
      base::StrCat({"NetworkService.IpProtection.TokenBatchGenerationTime.",
                    BlindSignAuthPhaseToString(phase)}),
      duration);
}

void IpProtectionTelemetryUma::TryGetAuthTokensError(uint32_t hash) {
  base::UmaHistogramSparse("NetworkService.IpProtection.TryGetAuthTokensErrors",
                           hash);
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

void IpProtectionTelemetryUma::TokenExpirationRate(ProxyLayer proxy_layer,
                                                   int value) {
  base::UmaHistogramCounts100000(
      base::StrCat({"NetworkService.IpProtection.",
                    ProxyLayerToString(proxy_layer), ".TokenExpirationRate"}),
      value);
}

void IpProtectionTelemetryUma::MdlEstimatedMemoryUsage(size_t usage) {
  base::UmaHistogramCustomCounts(
      "NetworkService.MaskedDomainList.EstimatedMemoryUsage",
      usage / 1024,  // Convert to KB
      /*min=*/1,
      /*exclusive_max=*/5000,  // Maximum of 5MB
      /*buckets=*/50);
}

void IpProtectionTelemetryUma::MdlEstimatedDiskUsage(int64_t usage) {
  base::UmaHistogramCustomCounts("NetworkService.MaskedDomainList.DiskUsage",
                                 usage / 1024,  // Convert to KB
                                 /*min=*/1,
                                 /*exclusive_max=*/5000,  // Maximum of 5MB
                                 /*buckets=*/50);
}

void IpProtectionTelemetryUma::MdlSize(int64_t size) {
  base::UmaHistogramCustomCounts("NetworkService.MaskedDomainList.Size2",
                                 size / 1024,  // Convert to KB
                                 /*min=*/1,
                                 /*exclusive_max=*/5000,  // Maximum of 5MB
                                 /*buckets=*/50);
}

void IpProtectionTelemetryUma::MdlFlatbufferBuildTime(
    base::TimeDelta duration) {
  base::UmaHistogramTimes(
      "NetworkService.IpProtection.ProxyAllowList.FlatbufferBuildTime",
      duration);
}

void IpProtectionTelemetryUma::MdlUpdateSuccess(bool success) {
  base::UmaHistogramBoolean(
      "NetworkService.IpProtection.ProxyAllowList.UpdateSuccess", success);
}

void IpProtectionTelemetryUma::MdlFirstUpdateTime(base::TimeDelta duration) {
  base::UmaHistogramTimes("NetworkService.MaskedDomainList.FirstUpdateTime",
                          duration);
}

void IpProtectionTelemetryUma::MdlMatchesTime(base::TimeDelta duration) {
  base::UmaHistogramMicrosecondsTimes(
      "NetworkService.MaskedDomainList.MatchesTime", duration);
}

void IpProtectionTelemetryUma::QuicProxiesFailed(int after_requests) {
  base::UmaHistogramCounts1000("NetworkService.IpProtection.QuicProxiesFailed",
                               after_requests);
}

void IpProtectionTelemetryUma::RecordTokenCountEvent(
    ProxyLayer layer,
    IpProtectionTokenCountEvent event,
    int count) {
  // Construct the histogram name dynamically based on the layer and event type.
  // Example: "NetworkService.IpProtection.ProxyA.TokenCount.Issued"
  std::string histogram_name = base::StrCat({
      "NetworkService.IpProtection.",
      ProxyLayerToString(layer),
      ".TokenCount.",
      TokenCountEventToString(event),
  });

  // Using a maximum of 1000 counts, since the maximum number of tokens per
  // event would generally be around the batch or cache size which is typically
  // much less than 1000.
  base::UmaHistogramCounts1000(histogram_name, count);
}

void IpProtectionTelemetryUma::TokenDemandDuringBatchGeneration(int count) {
  base::UmaHistogramCounts100(
      "NetworkService.IpProtection.TokenDemandDuringBatchGeneration", count);
}

void IpProtectionTelemetryUma::RecordStreamCreationAttemptedMetrics(
    const net::ProxyChain& proxy_chain,
    base::TimeDelta duration,
    base::optional_ref<int> net_error) {
  CHECK(proxy_chain.is_for_ip_protection());
  const std::string suffix = proxy_chain.GetHistogramSuffix();

  base::UmaHistogramTimes(
      base::StrCat({"Net.IpProtection.StreamCreation",
                    net_error.has_value() ? "ErrorTime." : "SuccessTime.",
                    suffix}),
      duration);

  if (net_error.has_value()) {
    base::UmaHistogramSparse(
        base::StrCat({"Net.IpProtection.StreamCreationError.", suffix}),
        net_error.value());
  }
}

}  // namespace ip_protection
