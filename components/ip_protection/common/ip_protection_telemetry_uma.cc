// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_telemetry_uma.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
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
                "`ip_protection::ProxyChainId`");
  CHECK(chain_id >= static_cast<int>(ProxyChainId::kUnknown) &&
        chain_id <= static_cast<int>(ProxyChainId::kMaxValue));
  return static_cast<ProxyChainId>(chain_id);
}

}  // namespace

IpProtectionTelemetry& Telemetry() {
  static IpProtectionTelemetryUma instance;
  return instance;
}

void IpProtectionTelemetryUma::OAuthTokenFetchComplete(base::TimeDelta value) {
  base::UmaHistogramTimes("NetworkService.IpProtection.OAuthTokenFetchTime",
                          value);
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

void IpProtectionTelemetryUma::ProxyChainFallback(int proxy_chain_id) {
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.ProxyChainFallback",
      ChainIdToEnum(proxy_chain_id));
}

}  // namespace ip_protection
