// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/components/tether/asynchronous_shutdown_object_container.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"

class PrefService;

namespace chromeos {

class ManagedNetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkStateHandler;

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace tether {

class NetworkConfigurationRemover;
class TetherHostFetcher;
class WifiHotspotDisconnector;

// Concrete AsynchronousShutdownObjectContainer implementation.
class AsynchronousShutdownObjectContainerImpl
    : public AsynchronousShutdownObjectContainer,
      public DisconnectTetheringRequestSender::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<AsynchronousShutdownObjectContainer> Create(
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        TetherHostFetcher* tether_host_fetcher,
        NetworkStateHandler* network_state_handler,
        ManagedNetworkConfigurationHandler*
            managed_network_configuration_handler,
        NetworkConnectionHandler* network_connection_handler,
        PrefService* pref_service);
    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<AsynchronousShutdownObjectContainer> CreateInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        TetherHostFetcher* tether_host_fetcher,
        NetworkStateHandler* network_state_handler,
        ManagedNetworkConfigurationHandler*
            managed_network_configuration_handler,
        NetworkConnectionHandler* network_connection_handler,
        PrefService* pref_service) = 0;
    virtual ~Factory();

   private:
    static Factory* factory_instance_;
  };

  ~AsynchronousShutdownObjectContainerImpl() override;

  // AsynchronousShutdownObjectContainer:
  void Shutdown(base::OnceClosure shutdown_complete_callback) override;
  TetherHostFetcher* tether_host_fetcher() override;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender()
      override;
  NetworkConfigurationRemover* network_configuration_remover() override;
  WifiHotspotDisconnector* wifi_hotspot_disconnector() override;

 protected:
  AsynchronousShutdownObjectContainerImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      TetherHostFetcher* tether_host_fetcher,
      NetworkStateHandler* network_state_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkConnectionHandler* network_connection_handler,
      PrefService* pref_service);

  // DisconnectTetheringRequestSender::Observer:
  void OnPendingDisconnectRequestsComplete() override;

 private:
  friend class AsynchronousShutdownObjectContainerImplTest;

  void ShutdownIfPossible();
  bool AreAsynchronousOperationsActive();

  void SetTestDoubles(std::unique_ptr<DisconnectTetheringRequestSender>
                          disconnect_tethering_request_sender);


  TetherHostFetcher* tether_host_fetcher_;
  std::unique_ptr<DisconnectTetheringRequestSender>
      disconnect_tethering_request_sender_;
  std::unique_ptr<NetworkConfigurationRemover> network_configuration_remover_;
  std::unique_ptr<WifiHotspotDisconnector> wifi_hotspot_disconnector_;

  // Not set until Shutdown() is invoked.
  base::OnceClosure shutdown_complete_callback_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousShutdownObjectContainerImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_
