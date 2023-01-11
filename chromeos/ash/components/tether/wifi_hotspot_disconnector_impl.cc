// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/wifi_hotspot_disconnector_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/network_configuration_remover.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace tether {

// static
void WifiHotspotDisconnectorImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDisconnectingWifiNetworkPath,
                               std::string());
}

WifiHotspotDisconnectorImpl::WifiHotspotDisconnectorImpl(
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler,
    PrefService* pref_service,
    NetworkConfigurationRemover* network_configuration_remover)
    : network_connection_handler_(network_connection_handler),
      network_state_handler_(network_state_handler),
      pref_service_(pref_service),
      network_configuration_remover_(network_configuration_remover) {
  std::string disconnecting_wifi_path_from_previous_session =
      pref_service_->GetString(prefs::kDisconnectingWifiNetworkPath);
  if (disconnecting_wifi_path_from_previous_session.empty())
    return;

  // If a previous disconnection attempt was aborted before it could be fully
  // completed, clean up the leftover network configuration.
  network_configuration_remover_->RemoveNetworkConfigurationByPath(
      disconnecting_wifi_path_from_previous_session);
  pref_service_->ClearPref(prefs::kDisconnectingWifiNetworkPath);
}

WifiHotspotDisconnectorImpl::~WifiHotspotDisconnectorImpl() = default;

void WifiHotspotDisconnectorImpl::DisconnectFromWifiHotspot(
    const std::string& wifi_network_guid,
    base::OnceClosure success_callback,
    StringErrorCallback error_callback) {
  const NetworkState* wifi_network_state =
      network_state_handler_->GetNetworkStateFromGuid(wifi_network_guid);
  if (!wifi_network_state) {
    PA_LOG(ERROR) << "Wi-Fi NetworkState for GUID " << wifi_network_guid << " "
                  << "does not exist. Cannot disconnect.";
    std::move(error_callback).Run(NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  if (!wifi_network_state->IsConnectedState()) {
    PA_LOG(ERROR) << "Wi-Fi NetworkState for GUID " << wifi_network_guid << " "
                  << "is not connected. Cannot disconnect.";
    std::move(error_callback).Run(NetworkConnectionHandler::kErrorNotConnected);
    return;
  }

  const std::string wifi_network_path = wifi_network_state->path();
  DCHECK(!wifi_network_path.empty());

  // Before starting disconnection, log the disconnecting Wi-Fi GUID to prefs.
  // Under normal circumstances, the GUID will be cleared as part of
  // CleanUpAfterWifiDisconnection(). However, when the user logs out,
  // this WifiHotspotDisconnectorImpl instance will be deleted before one of the
  // callbacks passed below to DisconnectNetwork() can be called, and the
  // GUID will remain in prefs until the next time the user logs in, at which
  // time the associated network configuration can be removed.
  pref_service_->Set(prefs::kDisconnectingWifiNetworkPath,
                     base::Value(wifi_network_path));

  auto split_callback = base::SplitOnceCallback(std::move(error_callback));
  network_connection_handler_->DisconnectNetwork(
      wifi_network_path,
      base::BindOnce(&WifiHotspotDisconnectorImpl::OnSuccessfulWifiDisconnect,
                     weak_ptr_factory_.GetWeakPtr(), wifi_network_guid,
                     wifi_network_path, std::move(success_callback),
                     std::move(split_callback.first)),
      base::BindOnce(&WifiHotspotDisconnectorImpl::OnFailedWifiDisconnect,
                     weak_ptr_factory_.GetWeakPtr(), wifi_network_guid,
                     wifi_network_path, std::move(split_callback.second)));
}

void WifiHotspotDisconnectorImpl::OnSuccessfulWifiDisconnect(
    const std::string& wifi_network_guid,
    const std::string& wifi_network_path,
    base::OnceClosure success_callback,
    StringErrorCallback error_callback) {
  PA_LOG(VERBOSE) << "Successfully disconnected from Wi-Fi network with GUID "
                  << wifi_network_guid << ".";
  CleanUpAfterWifiDisconnection(wifi_network_path, std::move(success_callback),
                                std::move(error_callback));
}

void WifiHotspotDisconnectorImpl::OnFailedWifiDisconnect(
    const std::string& wifi_network_guid,
    const std::string& wifi_network_path,
    StringErrorCallback error_callback,
    const std::string& error_name) {
  PA_LOG(ERROR) << "Failed to disconnect from Wi-Fi network with GUID "
                << wifi_network_guid << ". Error name: " << error_name;
  CleanUpAfterWifiDisconnection(wifi_network_path, base::OnceClosure(),
                                std::move(error_callback));
}

void WifiHotspotDisconnectorImpl::CleanUpAfterWifiDisconnection(
    const std::string& wifi_network_path,
    base::OnceClosure success_callback,
    StringErrorCallback error_callback) {
  network_configuration_remover_->RemoveNetworkConfigurationByPath(
      wifi_network_path);
  pref_service_->ClearPref(prefs::kDisconnectingWifiNetworkPath);

  if (success_callback)
    std::move(success_callback).Run();
  else
    std::move(error_callback)
        .Run(NetworkConnectionHandler::kErrorDisconnectFailed);
}

}  // namespace tether

}  // namespace ash
