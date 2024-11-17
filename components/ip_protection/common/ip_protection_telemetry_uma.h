// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_UMA_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_UMA_H_

#include <optional>

#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"

namespace ip_protection {

// Implementation of IpProtectionTelemetry using UMA.
class IpProtectionTelemetryUma final : public IpProtectionTelemetry {
 public:
  void OAuthTokenFetchComplete(base::TimeDelta) override;
  void TokenBatchFetchComplete(TryGetAuthTokensResult,
                               std::optional<base::TimeDelta>) override;
  void AndroidTokenBatchFetchComplete(
      TryGetAuthTokensAndroidResult result,
      std::optional<base::TimeDelta> duration) override;

  void ProxyChainFallback(int) override;
  void EmptyTokenCache(ProxyLayer) override;
  void ProxyResolution(ProxyResolutionResult) override;
  void GetAuthTokenResultForGeo(bool is_token_available,
                                bool enable_token_caching_by_geo,
                                bool is_cache_empty,
                                bool does_requested_geo_match_current) override;
  void TokenBatchGenerationComplete(base::TimeDelta duration) override;
  void GeoChangeTokenPresence(bool) override;
  void ProxyListRefreshComplete(
      GetProxyListResult result,
      std::optional<base::TimeDelta> duration) override;
  void TokenSpendRate(ProxyLayer, int) override;
  void TokenExpirationRate(ProxyLayer, int) override;
  void MdlEstimatedMemoryUsage(size_t) override;
  void AndroidAuthClientCreationTime(base::TimeDelta duration) override;
  void AndroidAuthClientGetInitialDataTime(base::TimeDelta duration) override;
  void AndroidAuthClientAuthAndSignTime(base::TimeDelta duration) override;
  void MdlFirstUpdateTime(base::TimeDelta duration) override;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_UMA_H_
