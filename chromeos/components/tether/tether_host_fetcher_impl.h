// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace tether {

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
    static std::unique_ptr<TetherHostFetcher> NewInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        chromeos::multidevice_setup::MultiDeviceSetupClient*
            multidevice_setup_client);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<TetherHostFetcher> BuildInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        chromeos::multidevice_setup::MultiDeviceSetupClient*
            multidevice_setup_client);

   private:
    static Factory* factory_instance_;
  };

  ~TetherHostFetcherImpl() override;

  // TetherHostFetcher:
  bool HasSyncedTetherHosts() override;
  void FetchAllTetherHosts(const TetherHostListCallback& callback) override;
  void FetchTetherHost(const std::string& device_id,
                       const TetherHostCallback& callback) override;

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
  TetherHostFetcherImpl(device_sync::DeviceSyncClient* device_sync_client,
                        chromeos::multidevice_setup::MultiDeviceSetupClient*
                            multidevice_setup_client_);

 private:
  void CacheCurrentTetherHosts();
  multidevice::RemoteDeviceRefList GenerateHostDeviceList();

  device_sync::DeviceSyncClient* device_sync_client_;
  chromeos::multidevice_setup::MultiDeviceSetupClient*
      multidevice_setup_client_;

  multidevice::RemoteDeviceRefList current_remote_device_list_;
  base::WeakPtrFactory<TetherHostFetcherImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcherImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
