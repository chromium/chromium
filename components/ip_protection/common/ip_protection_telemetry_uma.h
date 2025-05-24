// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_UMA_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_UMA_H_

#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"

namespace ip_protection {

enum class TryGetAuthTokensResult;
enum class TryGetAuthTokensAndroidResult;
enum class TryGetProbabilisticRevealTokensStatus;
enum class ProxyLayer;

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
                                bool is_cache_empty,
                                bool does_requested_geo_match_current) override;
  void TokenBatchGenerationComplete(base::TimeDelta duration) override;
  void TryGetAuthTokensError(uint32_t hash) override;
  void GeoChangeTokenPresence(bool) override;
  void ProxyListRefreshComplete(
      GetProxyListResult result,
      std::optional<base::TimeDelta> duration) override;
  void TokenSpendRate(ProxyLayer, int) override;
  void TokenExpirationRate(ProxyLayer, int) override;
  void MdlEstimatedMemoryUsage(size_t) override;
  void MdlEstimatedDiskUsage(int64_t) override;
  void MdlSize(int64_t) override;
  void AndroidAuthClientCreationTime(base::TimeDelta duration) override;
  void AndroidAuthClientGetInitialDataTime(base::TimeDelta duration) override;
  void AndroidAuthClientAuthAndSignTime(base::TimeDelta duration) override;
  void MdlFirstUpdateTime(base::TimeDelta duration) override;
  void MdlMatchesTime(base::TimeDelta duration) override;
  void GetProbabilisticRevealTokensComplete(
      TryGetProbabilisticRevealTokensStatus status,
      base::TimeDelta duration) override;
  void IsProbabilisticRevealTokenAvailable(bool is_initial_request,
                                           bool is_token_available) override;
  void ProbabilisticRevealTokenRandomizationTime(
      base::TimeDelta duration) override;
  void QuicProxiesFailed(int after_requests) override;
  void RecordTokenCountEvent(ProxyLayer layer,
                             IpProtectionTokenCountEvent event,
                             int count) override;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_UMA_H_
