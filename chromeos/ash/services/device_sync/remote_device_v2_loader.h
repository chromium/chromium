// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_V2_LOADER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_V2_LOADER_H_

#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry.h"

namespace ash {

namespace device_sync {

// Converts the given CryptAuthDevices into RemoteDevices.
class RemoteDeviceV2Loader {
 public:
  using LoadCallback =
      base::OnceCallback<void(const multidevice::RemoteDeviceList&)>;

  virtual ~RemoteDeviceV2Loader() = default;

  // Converts the input CryptAuthDevices to RemoteDevices.
  // |id_to_device_map|: A map from Instance ID to CryptAuthDevice which will be
  //     converted to a list of RemoteDevices.
  // |user_email|: The email of the user who owns the devices.
  // |user_private_key|: The private key of the user's local device. Used to
  //     derive the persistent symmetric key (PSK).
  // |callback|: Invoked when the conversion is complete.
  virtual void Load(
      const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& id_to_device_map,
      const std::string& user_email,
      const std::string& user_private_key,
      LoadCallback callback) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_V2_LOADER_H_
