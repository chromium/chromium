// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/synced_network_updater_impl.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/values.h"
#include "chromeos/components/sync_wifi/network_type_conversions.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "components/device_event_log/device_event_log.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

SyncedNetworkUpdaterImpl::SyncedNetworkUpdaterImpl(
    std::unique_ptr<PendingNetworkConfigurationTracker> tracker,
    network_config::mojom::CrosNetworkConfig* cros_network_config)
    : tracker_(std::move(tracker)), cros_network_config_(cros_network_config) {
  cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  // Load the current list of networks.
  OnNetworkStateListChanged();
}

SyncedNetworkUpdaterImpl::~SyncedNetworkUpdaterImpl() = default;

void SyncedNetworkUpdaterImpl::AddOrUpdateNetwork(
    const sync_pb::WifiConfigurationSpecificsData& specifics) {
  auto id = NetworkIdentifier::FromProto(specifics);
  network_config::mojom::NetworkStatePropertiesPtr existing_network =
      FindLocalNetwork(id);
  std::string change_guid = tracker_->TrackPendingUpdate(id, specifics);
  network_config::mojom::ConfigPropertiesPtr config =
      MojoNetworkConfigFromProto(specifics);

  if (existing_network) {
    cros_network_config_->SetProperties(
        existing_network->guid, std::move(config),
        base::BindOnce(&SyncedNetworkUpdaterImpl::OnSetPropertiesResult,
                       weak_ptr_factory_.GetWeakPtr(), change_guid, id));
    return;
  }
  cros_network_config_->ConfigureNetwork(
      std::move(config), /* shared= */ false,
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnConfigureNetworkResult,
                     weak_ptr_factory_.GetWeakPtr(), change_guid, id));
}

void SyncedNetworkUpdaterImpl::RemoveNetwork(const NetworkIdentifier& id) {
  network_config::mojom::NetworkStatePropertiesPtr network =
      FindLocalNetwork(id);
  if (!network)
    return;

  std::string change_guid =
      tracker_->TrackPendingUpdate(id, /*specifics=*/base::nullopt);

  cros_network_config_->ForgetNetwork(
      network->guid,
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnForgetNetworkResult,
                     weak_ptr_factory_.GetWeakPtr(), change_guid, id));
}

network_config::mojom::NetworkStatePropertiesPtr
SyncedNetworkUpdaterImpl::FindLocalNetwork(const NetworkIdentifier& id) {
  for (const network_config::mojom::NetworkStatePropertiesPtr& network :
       networks_) {
    if (id == NetworkIdentifier::FromMojoNetwork(network))
      return network.Clone();
  }
  return nullptr;
}

void SyncedNetworkUpdaterImpl::OnNetworkStateListChanged() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kConfigured,
          network_config::mojom::NetworkType::kWiFi,
          /* limit= */ 0),
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnGetNetworkList,
                     base::Unretained(this)));
}

void SyncedNetworkUpdaterImpl::OnGetNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  networks_ = std::move(networks);
}

void SyncedNetworkUpdaterImpl::OnError(const std::string& change_guid,
                                       const NetworkIdentifier& id,
                                       const std::string& error_name) {
  NET_LOG(ERROR) << "Failed to update id:" << id.SerializeToString()
                 << " error:" << error_name;
}

void SyncedNetworkUpdaterImpl::OnConfigureNetworkResult(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    const base::Optional<std::string>& guid,
    const std::string& error_message) {
  if (!guid) {
    OnError(change_guid, id, "Failed to configure network.");
    return;
  }
  VLOG(1) << "Successfully updated network with id " << id.SerializeToString();
  CleanupUpdate(change_guid, id);
}

void SyncedNetworkUpdaterImpl::OnSetPropertiesResult(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    bool success,
    const std::string& error_message) {
  if (!success) {
    OnError(change_guid, id, "Failed to update properties on network.");
    return;
  }
  VLOG(1) << "Successfully updated network with id " << id.SerializeToString();
  CleanupUpdate(change_guid, id);
}

void SyncedNetworkUpdaterImpl::OnForgetNetworkResult(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    bool success) {
  if (!success) {
    OnError(change_guid, id, "Failed to remove network.");
    return;
  }

  VLOG(1) << "Successfully deleted network with id " << id.SerializeToString();
  CleanupUpdate(change_guid, id);
}

void SyncedNetworkUpdaterImpl::CleanupUpdate(const std::string& change_guid,
                                             const NetworkIdentifier& id) {
  tracker_->MarkComplete(change_guid, id);
}

}  // namespace sync_wifi

}  // namespace chromeos
