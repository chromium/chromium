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
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_UMA_H_
