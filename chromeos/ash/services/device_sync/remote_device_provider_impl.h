// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_manager.h"
#include "chromeos/ash/services/device_sync/remote_device_provider.h"
#include "google_apis/gaia/core_account_id.h"

namespace ash {

namespace device_sync {

class CryptAuthDeviceSyncResult;
class RemoteDeviceV2Loader;

// Concrete RemoteDeviceProvider implementation that handles v2 DeviceSync data.
class RemoteDeviceProviderImpl : public RemoteDeviceProvider,
                                 public CryptAuthV2DeviceManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<RemoteDeviceProvider> Create(
        CryptAuthV2DeviceManager* v2_device_manager,
        const std::string& user_email,
        const std::string& user_private_key);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<RemoteDeviceProvider> CreateInstance(
        CryptAuthV2DeviceManager* v2_device_manager,
        const std::string& user_email,
        const std::string& user_private_key) = 0;

   private:
    static Factory* factory_instance_;
  };

  RemoteDeviceProviderImpl(CryptAuthV2DeviceManager* v2_device_manager,
                           const std::string& user_email,
                           const std::string& user_private_key);

  RemoteDeviceProviderImpl(const RemoteDeviceProviderImpl&) = delete;
  RemoteDeviceProviderImpl& operator=(const RemoteDeviceProviderImpl&) = delete;

  ~RemoteDeviceProviderImpl() override;

  // RemoteDeviceProvider:
  const multidevice::RemoteDeviceList& GetSyncedDevices() const override;

  // CryptAuthV2DeviceManager::Observer:
  void OnDeviceSyncFinished(
      const CryptAuthDeviceSyncResult& device_sync_result) override;

 private:
  void LoadV2RemoteDevices();

  void OnV2RemoteDevicesLoaded(
      const multidevice::RemoteDeviceList& synced_v2_remote_devices);

  // Used to retrieve CryptAuthDevices from the last v2 DeviceSync. Null if v2
  // DeviceSync is disabled.
  raw_ptr<CryptAuthV2DeviceManager> v2_device_manager_;

  // The email of the current user.
  const std::string user_email_;

  // The private key used to generate RemoteDevices.
  const std::string user_private_key_;

  std::unique_ptr<RemoteDeviceV2Loader> remote_device_v2_loader_;

  multidevice::RemoteDeviceList synced_remote_devices_;
  base::WeakPtrFactory<RemoteDeviceProviderImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_
