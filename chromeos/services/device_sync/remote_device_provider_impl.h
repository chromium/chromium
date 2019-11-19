// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_

#include "chromeos/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/services/device_sync/remote_device_provider.h"

namespace chromeos {

namespace device_sync {

class RemoteDeviceLoader;

// Concrete RemoteDeviceProvider implementation.
class RemoteDeviceProviderImpl : public RemoteDeviceProvider,
                                 public CryptAuthDeviceManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<RemoteDeviceProvider> NewInstance(
        CryptAuthDeviceManager* device_manager,
        const std::string& user_id,
        const std::string& user_private_key);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<RemoteDeviceProvider> BuildInstance(
        CryptAuthDeviceManager* device_manager,
        const std::string& user_id,
        const std::string& user_private_key);

   private:
    static Factory* factory_instance_;
  };

  RemoteDeviceProviderImpl(CryptAuthDeviceManager* device_manager,
                           const std::string& user_id,
                           const std::string& user_private_key);

  ~RemoteDeviceProviderImpl() override;

  // RemoteDeviceProvider:
  const multidevice::RemoteDeviceList& GetSyncedDevices() const override;

  // CryptAuthDeviceManager::Observer:
  void OnSyncFinished(
      CryptAuthDeviceManager::SyncResult sync_result,
      CryptAuthDeviceManager::DeviceChangeResult device_change_result) override;

 private:
  void OnRemoteDevicesLoaded(
      const multidevice::RemoteDeviceList& synced_remote_devices);

  // To get cryptauth::ExternalDeviceInfo needed to retrieve RemoteDevices.
  CryptAuthDeviceManager* device_manager_;

  // The account ID of the current user.
  const std::string user_id_;

  // The private key used to generate RemoteDevices.
  const std::string user_private_key_;

  std::unique_ptr<RemoteDeviceLoader> remote_device_loader_;
  multidevice::RemoteDeviceList synced_remote_devices_;
  base::WeakPtrFactory<RemoteDeviceProviderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceProviderImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_
