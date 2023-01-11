// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

namespace ash {

namespace device_sync {

// Handles the BatchNotifyGroupDevices portion of the CryptAuth v2 DeviceSync
// protocol, which sends a GCM message--via CryptAuth--to a subset of devices in
// the "DeviceSync:BetterTogether" group.
class CryptAuthDeviceNotifier {
 public:
  CryptAuthDeviceNotifier() = default;

  CryptAuthDeviceNotifier(const CryptAuthDeviceNotifier&) = delete;
  CryptAuthDeviceNotifier& operator=(const CryptAuthDeviceNotifier&) = delete;

  virtual ~CryptAuthDeviceNotifier() = default;

  // Sends a GCM message to devices with Instance IDs |device_ids|. The message
  // includes the CryptAuth service--Enrollment or DeviceSync--and the feature
  // type--kBetterTogetherHostEnabled, for example--indicating the reason for
  // the notification; these are specified by |target_service| and
  // |feature_type|, respectively.
  virtual void NotifyDevices(
      const base::flat_set<std::string>& device_ids,
      cryptauthv2::TargetService target_service,
      CryptAuthFeatureType feature_type,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  //  CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_H_
