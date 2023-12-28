// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PROVISIONING_CONFIG_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PROVISIONING_CONFIG_FETCHER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/carrier_lock/carrier_lock.pb.h"
#include "chromeos/ash/components/carrier_lock/common.h"

namespace ash::carrier_lock {

struct RestrictedNetworks {
  int allowed;
  int disallowed;
};

// This class handles communication with Carrier Lock (SimLock) provisioning
// server managed by Pixel team in order to receive configuration for modem.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    ProvisioningConfigFetcher {
 public:
  ProvisioningConfigFetcher() = default;
  virtual ~ProvisioningConfigFetcher() = default;

  // Send request to provisioning server for latest Carrier Lock configuration.
  virtual void RequestConfig(const std::string& serial,
                             const std::string& imei,
                             const std::string& manufacturer,
                             const std::string& model,
                             const std::string& fcm_token,
                             Callback callback) = 0;

  // Return FCM topic from received Carrier Lock configuration.
  virtual std::string GetFcmTopic() = 0;

  // Return signed part of received lock configuration.
  virtual std::string GetSignedConfig() = 0;

  // Return default restriction mode.
  virtual ::carrier_lock::CarrierRestrictionsMode GetRestrictionMode() = 0;

  // Return number of allowed and disallowed networks in received configuration.
  virtual RestrictedNetworks GetNumberOfNetworks() = 0;
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PROVISIONING_CONFIG_FETCHER_H_
