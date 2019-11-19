// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_SETTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_SETTER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/services/device_sync/feature_status_change.h"
#include "chromeos/services/device_sync/network_request_error.h"

namespace chromeos {

namespace device_sync {

// Handles the BatchSetFeatureStatuses portion of the CryptAuth v2 DeviceSync
// protocol. While the BatchSetFeatureStatuses API allows for setting multiple
// feature statuses for multiple devices, we restrict our interface to a single
// feature for a single device. This is for simplicity as well as consistency
// with the CryptAuth v1 DeviceSync interface.
class CryptAuthFeatureStatusSetter {
 public:
  CryptAuthFeatureStatusSetter() = default;
  virtual ~CryptAuthFeatureStatusSetter() = default;

  // Enables or disables |feature| for the device with device ID |device_id|.
  virtual void SetFeatureStatus(
      const std::string& device_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptAuthFeatureStatusSetter);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_SETTER_H_
