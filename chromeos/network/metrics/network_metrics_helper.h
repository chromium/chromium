// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_
#define CHROMEOS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_

#include "base/component_export.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// Provides APIs for logging to general network metrics that apply to all
// network types and their variants.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkMetricsHelper {
 public:
  NetworkMetricsHelper();
  ~NetworkMetricsHelper();

  void Init(NetworkStateHandler* network_state_handler);

  // Logs connection result for network with given |guid|. If |shill_error| has
  // no value, a connection success is logged.
  void LogAllConnectionResult(
      const std::string& guid,
      const absl::optional<std::string>& shill_error = absl::nullopt);

 private:
  // Result of state changes to a network triggered by any connection
  // attempt. With the exception of kSuccess and kUnknown, these enums are
  // mapped directly to Shill errors. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never be reused.
  enum class ShillConnectResult {
    kSuccess = 0,
    kUnknown = 1,
    kFailedToConnect = 2,
    kDhcpFailure = 3,
    kDnsLookupFailure = 4,
    kEapAuthentication = 5,
    kEapLocalTls = 6,
    kEapRemoteTls = 7,
    kOutOfRange = 8,
    kPinMissing = 9,
    kNoFailure = 10,
    kNotAssociated = 11,
    kNotAuthenticated = 12,
    kTooManySTAs = 13,
    kBadPassphrase = 14,
    kBadWepKey = 15,
    kErrorSimLocked = 16,
    kErrorNotRegistered = 17,
    kMaxValue = kErrorNotRegistered,
  };

  ShillConnectResult ShillErrorToConnectResult(const std::string& error_name);

  NetworkStateHandler* network_state_handler_;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_
