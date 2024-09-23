// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SOFTWARE_FEATURE_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SOFTWARE_FEATURE_MANAGER_H_

#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"

namespace ash {

namespace device_sync {

// Queries for eligible MultiDevice hosts and sets/changes/unsets the current
// MultiDevice host for the logged-in account.
class SoftwareFeatureManager {
 public:
  virtual ~SoftwareFeatureManager() {}

  // Enables or disables |software_feature| for the device with public key
  // |public_key|. If |enabled| and |is_exclusive| are both true, then all other
  // devices associated with this account will have |sofware_feature| disabled.
  // |is_exclusive| is ignored if |enabled| is false.
  //
  // Note: In the special case of passing |software_feature| =
  // SoftwareFeature::EASY_UNLOCK_HOST and |enabled| = false, |public_key| is
  // ignored.
  virtual void SetSoftwareFeatureState(
      const std::string& public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback,
      bool is_exclusive = false) = 0;

  // Enables or disables |feature| for the device with Instance ID |device_id|.
  // This is the v2 DeviceSync analog of SetSoftwareFeatureState. This function
  // should only be used when v1 and v2 DeviceSync are both active so that
  // SetSoftwareFeatureState() and SetFeatureStatus() calls can be ordered.
  // TODO(crbug.com/40105247): Remove when v1 DeviceSync is disabled and
  // CryptAuthFeatureStatusSetter is called directly.
  virtual void SetFeatureStatus(
      const std::string& device_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) = 0;

  // Finds eligible devices associated with the logged-in account which support
  // |software_feature|.
  virtual void FindEligibleDevices(
      multidevice::SoftwareFeature software_feature,
      base::OnceCallback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                              const std::vector<cryptauth::IneligibleDevice>&)>
          success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SOFTWARE_FEATURE_MANAGER_H_
