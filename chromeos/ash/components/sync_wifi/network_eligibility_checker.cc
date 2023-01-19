// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/network_eligibility_checker.h"

#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"

namespace ash::network_config {
namespace mojom = ::chromeos::network_config::mojom;
}

namespace ash::sync_wifi {

NetworkEligibilityStatus GetNetworkEligibilityStatus(
    const std::string& guid,
    bool is_connectable,
    bool is_hidden,
    const network_config::mojom::SecurityType& security_type,
    const network_config::mojom::OncSource& source,
    bool log_result) {
  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::IsInitialized()
          ? NetworkHandler::Get()->network_metadata_store()
          : nullptr;
  if (!network_metadata_store)
    return NetworkEligibilityStatus::kNoMetadata;

  if (source == network_config::mojom::OncSource::kDevicePolicy ||
      source == network_config::mojom::OncSource::kUserPolicy) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, configured by policy.";
    }
    return NetworkEligibilityStatus::kProhibitedByPolicy;
  }

  if (network_metadata_store->GetHasBadPassword(guid) &&
      network_metadata_store->GetLastConnectedTimestamp(guid).is_zero()) {
    if (log_result) {
      NET_LOG(EVENT)
          << NetworkGuidId(guid)
          << " is not eligible, it has a bad password and has never connected.";
    }
    return NetworkEligibilityStatus::kInvalidPassword;
  }

  if (!is_connectable) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, it is not connectable.";
    }
    return NetworkEligibilityStatus::kNotConnectable;
  }

  if (is_hidden) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, it is hidden.";
    }
    return NetworkEligibilityStatus::kHiddenSsid;
  }

  if (!network_metadata_store->GetIsCreatedByUser(guid)) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, was not configured by user.";
    }
    return NetworkEligibilityStatus::kNotConfiguredByUser;
  }

  if (security_type != network_config::mojom::SecurityType::kWepPsk &&
      security_type != network_config::mojom::SecurityType::kWpaPsk) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, security type not supported: "
                     << security_type;
    }
    return NetworkEligibilityStatus::kUnsupportedSecurityType;
  }

  if (log_result) {
    NET_LOG(EVENT) << NetworkGuidId(guid) << " is eligible for sync.";
  }
  return NetworkEligibilityStatus::kNetworkIsEligible;
}

bool IsEligibleForSync(const std::string& guid,
                       bool is_connectable,
                       bool is_hidden,
                       const network_config::mojom::SecurityType& security_type,
                       const network_config::mojom::OncSource& source,
                       bool log_result) {
  return GetNetworkEligibilityStatus(guid, is_connectable, is_hidden,
                                     security_type, source, log_result) ==
         NetworkEligibilityStatus::kNetworkIsEligible;
}

}  // namespace ash::sync_wifi
