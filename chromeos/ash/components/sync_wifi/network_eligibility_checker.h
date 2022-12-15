// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_NETWORK_ELIGIBILITY_CHECKER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_NETWORK_ELIGIBILITY_CHECKER_H_

#include <string>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"

namespace ash::sync_wifi {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NetworkEligibilityStatus {
  kNetworkIsEligible = 0,
  kNoMetadata = 1,
  kProhibitedByPolicy = 2,
  kInvalidPassword = 3,
  kNotConnectable = 4,
  kNotConfiguredByUser = 5,
  kUnsupportedSecurityType = 6,
  kNoWifiNetworksAvailable = 7,
  kHiddenSsid = 8,
  kMaxValue = kHiddenSsid
};

NetworkEligibilityStatus GetNetworkEligibilityStatus(
    const std::string& guid,
    bool is_connectable,
    bool is_hidden,
    const chromeos::network_config::mojom::SecurityType& security_type,
    const chromeos::network_config::mojom::OncSource& source,
    bool log_result);

bool IsEligibleForSync(
    const std::string& guid,
    bool is_connectable,
    bool is_hidden,
    const chromeos::network_config::mojom::SecurityType& security_type,
    const chromeos::network_config::mojom::OncSource& source,
    bool log_result);

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_NETWORK_ELIGIBILITY_CHECKER_H_
