// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_device_manager.h"
#include "chromeos/ash/services/device_sync/remote_device_provider.h"
#include "google_apis/gaia/core_account_id.h"

namespace ash {

namespace device_sync {

class CryptAuthDeviceSyncResult;
class RemoteDeviceLoader;
class RemoteDeviceV2Loader;

// Concrete RemoteDeviceProvider implementation that handles v1 and/or v2
// DeviceSync data.
//
// If v1 and v2 DeviceSync are running in parallel, we consider the following
// facts:
//  - Devices returned from a v2 DeviceSync should be a subset of the devices
//    returned from a v1 DeviceSync.
//  - The public key is the only consistent identifier between v1 and v2
//    devices.
//  - V2 devices might not have a decrypted public key or decrypted beacon
//    seeds.
//  - V1 and v2 device data might have different beacon seeds.
// Given these facts, we merge the v1 and v2 devices in the following way so as
// to avoid having duplicate device records and to ensure that v2 device data is
// preferred if it has decrypted metadata:
//  - Initially, populate the synced-device list with all v1 devices.
//  - If a v2 device does not have a public key, ignore it.
//  - If a v2 device has a public key that matches that of a v1 device,
//    replace the v1 device with the v2 device in the synced-device list.
//  - If a v2 device has a public key that does not match any v1 device,
//    append it to the synced-device list.
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

  void OnV1RemoteDevicesLoaded(
      const multidevice::RemoteDeviceList& synced_v1_remote_devices);
  void OnV2RemoteDevicesLoaded(
      const multidevice::RemoteDeviceList& synced_v2_remote_devices);

  // Only invoked when running v1 and v2 DeviceSync in parallel.
  void MergeV1andV2SyncedDevices();

  // Used to retrieve CryptAuthDevices from the last v2 DeviceSync. Null if v2
  // DeviceSync is disabled.
  raw_ptr<CryptAuthV2DeviceManager> v2_device_manager_;

  // The email of the current user.
  const std::string user_email_;

  // The private key used to generate RemoteDevices.
  const std::string user_private_key_;

  std::unique_ptr<RemoteDeviceLoader> remote_device_v1_loader_;
  std::unique_ptr<RemoteDeviceV2Loader> remote_device_v2_loader_;

  // Only populated when running v1 and v2 DeviceSync in parallel.
  multidevice::RemoteDeviceList synced_v1_remote_devices_to_be_merged_;
  multidevice::RemoteDeviceList synced_v2_remote_devices_to_be_merged_;

  multidevice::RemoteDeviceList synced_remote_devices_;
  base::WeakPtrFactory<RemoteDeviceProviderImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_REMOTE_DEVICE_PROVIDER_IMPL_H_
