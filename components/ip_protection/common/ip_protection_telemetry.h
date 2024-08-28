// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_H_

#include <optional>

#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"

namespace ip_protection {

// An abstract interface for all of the telemetry associated with IP Protection.
//
// This is implemented by each telemetry platform, and a singleton made
// available on a per-process basis.
class IpProtectionTelemetry {
 public:
  virtual ~IpProtectionTelemetry() = default;

  // Methods to make measurements of specific events related to IP Protection.

  // An OAuth token was fetched successfully, for purposes of authenticating
  // calls to getInitialData and authAndSign (not for getProxyConfig).
  virtual void OAuthTokenFetchComplete(base::TimeDelta duration) = 0;

  // A token batch was fetched. If result is not `kSuccess`, then the duration
  // is nullopt.
  virtual void TokenBatchFetchComplete(
      TryGetAuthTokensResult result,
      std::optional<base::TimeDelta> duration) = 0;
};

// Get the singleton instance of this type. This will be implemented by each
// subclass, with only one being built on any particular platform.
IpProtectionTelemetry& Telemetry();

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TELEMETRY_H_
