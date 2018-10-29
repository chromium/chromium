// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_provider.h"
#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {
class RemoteDeviceProvider;
}  // namespace cryptauth

namespace chromeos {

namespace tether {

// Concrete TetherHostFetcher implementation. Despite the asynchronous function
// prototypes, callbacks are invoked synchronously.
//
// Note: TetherHostFetcherImpl, and the Tether feature as a whole, is currently
// in the middle of a migration from using RemoteDeviceProvider to
// DeviceSyncClient and eventually to MultiDeviceSetupClient. Its constructor
// accepts all three objects, but some may be null. (This is controlled at a
// higher level by features::kMultiDeviceApi and
// features::kEnableUnifiedMultiDeviceSetup.). Once Tether has been fully
// migrated, RemoteDeviceProvider and eventually DeviceSyncClient will be ripped
// out of this class. See https://crbug.com/848956.
class TetherHostFetcherImpl
    : public TetherHostFetcher,
      public cryptauth::RemoteDeviceProvider::Observer,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<TetherHostFetcher> NewInstance(
        cryptauth::RemoteDeviceProvider* remote_device_provider,
        device_sync::DeviceSyncClient* device_sync_client,
        chromeos::multidevice_setup::MultiDeviceSetupClient*
            multidevice_setup_client);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<TetherHostFetcher> BuildInstance(
        cryptauth::RemoteDeviceProvider* remote_device_provider,
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

  // cryptauth::RemoteDeviceProvider::Observer:
  void OnSyncDeviceListChanged() override;

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
  // TODO(crbug.com/848956): Remove RemoteDeviceProvider once all clients have
  // migrated to the DeviceSync Mojo API.
  TetherHostFetcherImpl(cryptauth::RemoteDeviceProvider* remote_device_provider,
                        device_sync::DeviceSyncClient* device_sync_client,
                        chromeos::multidevice_setup::MultiDeviceSetupClient*
                            multidevice_setup_client_);

 private:
  enum class TetherHostSource {
    UNKNOWN,
    MULTIDEVICE_SETUP_CLIENT,
    DEVICE_SYNC_CLIENT,
    REMOTE_DEVICE_PROVIDER
  };

  void CacheCurrentTetherHosts();
  cryptauth::RemoteDeviceRefList GenerateHostDeviceList();
  TetherHostSource GetTetherHostSourceBasedOnFlags();
  // This returns true if there is no BETTER_TOGETHER_HOST supported or enabled,
  // but there *are* MAGIC_TETHER_HOSTs supported or enabled. This can only
  // happen if the user's phone has not yet fully updated to the new multidevice
  // world.
  // TODO(crbug.com/894585): Remove this legacy special case after M71.
  bool IsInLegacyHostMode();

  cryptauth::RemoteDeviceProvider* remote_device_provider_;
  device_sync::DeviceSyncClient* device_sync_client_;
  chromeos::multidevice_setup::MultiDeviceSetupClient*
      multidevice_setup_client_;

  cryptauth::RemoteDeviceRefList current_remote_device_list_;
  base::WeakPtrFactory<TetherHostFetcherImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcherImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
