// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/asynchronous_shutdown_object_container_impl.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/tether/disconnect_tethering_request_sender_impl.h"
#include "chromeos/ash/components/tether/network_configuration_remover.h"
#include "chromeos/ash/components/tether/secure_channel_host_connection.h"
#include "chromeos/ash/components/tether/wifi_hotspot_disconnector_impl.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash::tether {

// static
AsynchronousShutdownObjectContainerImpl::Factory*
    AsynchronousShutdownObjectContainerImpl::Factory::factory_instance_ =
        nullptr;

// static
std::unique_ptr<AsynchronousShutdownObjectContainer>
AsynchronousShutdownObjectContainerImpl::Factory::Create(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    TetherHostFetcher* tether_host_fetcher,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    PrefService* pref_service) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        device_sync_client, secure_channel_client, tether_host_fetcher,
        network_state_handler, managed_network_configuration_handler,
        network_connection_handler, pref_service);
  }

  return base::WrapUnique(new AsynchronousShutdownObjectContainerImpl(
      device_sync_client, secure_channel_client, tether_host_fetcher,
      network_state_handler, managed_network_configuration_handler,
      network_connection_handler, pref_service));
}

// static
void AsynchronousShutdownObjectContainerImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

AsynchronousShutdownObjectContainerImpl::Factory::~Factory() = default;

AsynchronousShutdownObjectContainerImpl::
    AsynchronousShutdownObjectContainerImpl(
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        TetherHostFetcher* tether_host_fetcher,
        NetworkStateHandler* network_state_handler,
        ManagedNetworkConfigurationHandler*
            managed_network_configuration_handler,
        NetworkConnectionHandler* network_connection_handler,
        PrefService* pref_service)
    : tether_host_fetcher_(tether_host_fetcher),
      host_connection_factory_(base::WrapUnique<HostConnection::Factory>(
          new SecureChannelHostConnection::Factory(device_sync_client,
                                                   secure_channel_client,
                                                   tether_host_fetcher))),
      disconnect_tethering_request_sender_(
          DisconnectTetheringRequestSenderImpl::Factory::Create(
              host_connection_factory_.get(),
              tether_host_fetcher_)),
      network_configuration_remover_(
          std::make_unique<NetworkConfigurationRemover>(
              managed_network_configuration_handler)),
      wifi_hotspot_disconnector_(std::make_unique<WifiHotspotDisconnectorImpl>(
          network_connection_handler,
          network_state_handler,
          pref_service,
          network_configuration_remover_.get())) {}

AsynchronousShutdownObjectContainerImpl::
    ~AsynchronousShutdownObjectContainerImpl() = default;

void AsynchronousShutdownObjectContainerImpl::Shutdown(
    base::OnceClosure shutdown_complete_callback) {
  DCHECK(shutdown_complete_callback_.is_null());
  shutdown_complete_callback_ = std::move(shutdown_complete_callback);

  // The objects below require asynchronous shutdowns, so start observering
  // these objects. Once they notify observers that they are finished shutting
  // down, the asynchronous shutdown will complete.
  disconnect_tethering_request_sender_->AddObserver(this);

  ShutdownIfPossible();
}

TetherHostFetcher*
AsynchronousShutdownObjectContainerImpl::tether_host_fetcher() {
  return tether_host_fetcher_;
}

HostConnection::Factory*
AsynchronousShutdownObjectContainerImpl::host_connection_factory() {
  return host_connection_factory_.get();
}

DisconnectTetheringRequestSender*
AsynchronousShutdownObjectContainerImpl::disconnect_tethering_request_sender() {
  return disconnect_tethering_request_sender_.get();
}

NetworkConfigurationRemover*
AsynchronousShutdownObjectContainerImpl::network_configuration_remover() {
  return network_configuration_remover_.get();
}

WifiHotspotDisconnector*
AsynchronousShutdownObjectContainerImpl::wifi_hotspot_disconnector() {
  return wifi_hotspot_disconnector_.get();
}

void AsynchronousShutdownObjectContainerImpl::
    OnPendingDisconnectRequestsComplete() {
  ShutdownIfPossible();
}

void AsynchronousShutdownObjectContainerImpl::ShutdownIfPossible() {
  DCHECK(!shutdown_complete_callback_.is_null());

  if (AreAsynchronousOperationsActive()) {
    return;
  }

  disconnect_tethering_request_sender_->RemoveObserver(this);

  std::move(shutdown_complete_callback_).Run();
}

bool AsynchronousShutdownObjectContainerImpl::
    AreAsynchronousOperationsActive() {
  // If there are pending disconnection requests, they must be sent before the
  // component shuts down.
  if (disconnect_tethering_request_sender_->HasPendingRequests()) {
    return true;
  }

  return false;
}

void AsynchronousShutdownObjectContainerImpl::SetTestDoubles(
    std::unique_ptr<DisconnectTetheringRequestSender>
        disconnect_tethering_request_sender) {
  disconnect_tethering_request_sender_ =
      std::move(disconnect_tethering_request_sender);
}

}  // namespace ash::tether
