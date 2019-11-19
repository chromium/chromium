// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_V2_LOADER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_V2_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/services/device_sync/cryptauth_device.h"
#include "chromeos/services/device_sync/cryptauth_device_registry.h"

namespace chromeos {

namespace multidevice {
class SecureMessageDelegate;
}  // namespace multidevice

namespace device_sync {

// Converts the given CryptAuthDevices into RemoteDevices. Some RemoteDevice
// fields are left empty if the CryptAuthDevice does not have
// CryptAuthBetterTogetherDeviceMetadata, for instance, if the metadata cannot
// be decrypted. If the public key is available for a device, a persistent
// symmetric key (PSK) is derived and added to the RemoteDevice; otherwise, the
// PSK is set to an empty string.
//
// A RemoteDeviceV2Loader object is designed to be used for only one Load()
// call. For a new attempt, a new object should be created. Note: The async
// calls to SecureMessage are guarded by the default DBus timeout (currently
// 25s).
class RemoteDeviceV2Loader {
 public:
  using LoadCallback =
      base::OnceCallback<void(const multidevice::RemoteDeviceList&)>;

  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<RemoteDeviceV2Loader> BuildInstance();

   private:
    static Factory* test_factory_;
  };

  virtual ~RemoteDeviceV2Loader();

  // Converts the input CryptAuthDevices to RemoteDevices. All devices are
  // converted but some remote device information might be missing, for
  // instance, if the CryptAuthBetterTogetherMetadata is not available.
  // |id_to_device_map|: A map from Instance ID to CryptAuthDevice which will be
  //     converted to a list of RemoteDevices.
  // |user_id|: The account ID of the user who owns the devices.
  // |user_private_key|: The private key of the user's local device. Used to
  //     derive the persistent symmetric key (PSK).
  // |callback|: Invoked when the conversion is complete.
  virtual void Load(
      const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& id_to_device_map,
      const std::string& user_id,
      const std::string& user_private_key,
      LoadCallback callback);

 private:
  RemoteDeviceV2Loader();

  // Disallow copy and assign.
  RemoteDeviceV2Loader(const RemoteDeviceV2Loader&) = delete;
  RemoteDeviceV2Loader& operator=(const RemoteDeviceV2Loader&) = delete;

  void OnPskDerived(const CryptAuthDevice& device,
                    const std::string& user_id,
                    const std::string& psk);
  void AddRemoteDevice(const CryptAuthDevice& device,
                       const std::string& user_id,
                       const std::string& psk);

  LoadCallback callback_;
  CryptAuthDeviceRegistry::InstanceIdToDeviceMap id_to_device_map_;
  base::flat_set<std::string> remaining_ids_to_process_;
  multidevice::RemoteDeviceList remote_devices_;
  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_V2_LOADER_H_
