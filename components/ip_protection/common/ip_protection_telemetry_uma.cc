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

}  // namespace ip_protection
