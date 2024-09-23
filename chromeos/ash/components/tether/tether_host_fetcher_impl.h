// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace ash::tether {

// Concrete TetherHostFetcher implementation. Despite the asynchronous function
// prototypes, callbacks are invoked synchronously.
//
// Note: TetherHostFetcherImpl, and the Tether feature as a whole, is currently
// in the middle of a migration from using DeviceSyncClient and eventually to
// MultiDeviceSetupClient. Its constructor accepts both objects, but either may
// be null. Once Tether has been fully migrated, DeviceSyncClient will be ripped
// out of this class. See https://crbug.com/848956.
class TetherHostFetcherImpl
    : public TetherHostFetcher,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<TetherHostFetcher> Create(
        device_sync::DeviceSyncClient* device_sync_client,
        multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<TetherHostFetcher> CreateInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        multidevice_setup::MultiDeviceSetupClient*
            multidevice_setup_client) = 0;

   private:
    static Factory* factory_instance_;
  };

  TetherHostFetcherImpl(const TetherHostFetcherImpl&) = delete;
  TetherHostFetcherImpl& operator=(const TetherHostFetcherImpl&) = delete;

  ~TetherHostFetcherImpl() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;
  void OnReady() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_status_with_device) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

 protected:
  TetherHostFetcherImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_);

 private:
  void CacheCurrentTetherHost();
  std::optional<multidevice::RemoteDeviceRef> GenerateTetherHost();

  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;

  base::WeakPtrFactory<TetherHostFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
