// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/synced_network_updater_impl.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/sync_wifi/network_type_conversions.h"
#include "chromeos/ash/components/timer_factory/timer_factory.h"
#include "components/device_event_log/device_event_log.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_config {
namespace mojom = ::chromeos::network_config::mojom;
}

namespace ash::sync_wifi {

namespace {

const int kMaxRetries = 3;
constexpr base::TimeDelta kTimeout = base::Minutes(1);

}  // namespace

SyncedNetworkUpdaterImpl::SyncedNetworkUpdaterImpl(
    std::unique_ptr<PendingNetworkConfigurationTracker> tracker,
    network_config::mojom::CrosNetworkConfig* cros_network_config,
    ash::timer_factory::TimerFactory* timer_factory,
    SyncedNetworkMetricsLogger* metrics_logger)
    : tracker_(std::move(tracker)),
      cros_network_config_(cros_network_config),
      timer_factory_(timer_factory),
      metrics_logger_(metrics_logger) {
  cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  // Load the current list of networks.
  OnNetworkStateListChanged();
  std::vector<PendingNetworkConfigurationUpdate> updates =
      tracker_->GetPendingUpdates();
  for (const PendingNetworkConfigurationUpdate& update : updates)
    Retry(update);
}

SyncedNetworkUpdaterImpl::~SyncedNetworkUpdaterImpl() = default;

void SyncedNetworkUpdaterImpl::AddOrUpdateNetwork(
    const sync_pb::WifiConfigurationSpecifics& specifics) {
  auto id = NetworkIdentifier::FromProto(specifics);
  std::string change_guid = tracker_->TrackPendingUpdate(id, specifics);
  StartAddOrUpdateOperation(change_guid, id, specifics);
}

void SyncedNetworkUpdaterImpl::StartAddOrUpdateOperation(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    const sync_pb::WifiConfigurationSpecifics& specifics) {
  network_config::mojom::NetworkStatePropertiesPtr existing_network =
      FindMojoNetwork(id);
  network_config::mojom::ConfigPropertiesPtr config =
      MojoNetworkConfigFromProto(specifics);

  // We can get into this state when the SSID's are improperly encoded hex
  // strings, which causes the proto conversion above to return a nullptr.
  //
  // TODO(b/270151177) : Dig into to understand why we are getting improperly
  // encoded SSIDs.
  if (!config) {
    NET_LOG(ERROR) << "Failed to generate local network config";
    metrics_logger_->RecordApplyGenerateLocalNetworkConfig(/*success=*/false);
    return;
  }

  metrics_logger_->RecordApplyGenerateLocalNetworkConfig(/*success=*/true);
  StartTimer(change_guid, id);

  if (existing_network) {
    NET_LOG(EVENT) << "Updating existing network "
                   << NetworkGuidId(existing_network->guid);
    if (network_guid_to_updates_counter_.contains(existing_network->guid))
      network_guid_to_updates_counter_[existing_network->guid]++;
    else
      network_guid_to_updates_counter_[existing_network->guid] = 1;
    cros_network_config_->SetProperties(
        existing_network->guid, std::move(config),
        base::BindOnce(&SyncedNetworkUpdaterImpl::OnSetPropertiesResult,
                       weak_ptr_factory_.GetWeakPtr(), change_guid,
                       existing_network->guid, specifics));
    return;
  }

  NET_LOG(EVENT) << "Adding new network configuration.";
  cros_network_config_->ConfigureNetwork(
      std::move(config), /*shared=*/false,
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnConfigureNetworkResult,
                     weak_ptr_factory_.GetWeakPtr(), change_guid, specifics));
}

void SyncedNetworkUpdaterImpl::RemoveNetwork(const NetworkIdentifier& id) {
  network_config::mojom::NetworkStatePropertiesPtr network =
      FindMojoNetwork(id);
  if (!network) {
    NET_LOG(EVENT) << "Network not found, nothing to remove.";
    return;
  }

  NET_LOG(EVENT) << "Removing network " << NetworkGuidId(network->guid);
  std::string change_guid =
      tracker_->TrackPendingUpdate(id, /*specifics=*/std::nullopt);
  StartDeleteOperation(change_guid, id, network->guid);
}

void SyncedNetworkUpdaterImpl::StartDeleteOperation(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    std::string guid) {
  StartTimer(change_guid, id);
  cros_network_config_->ForgetNetwork(
      guid, base::BindOnce(&SyncedNetworkUpdaterImpl::OnForgetNetworkResult,
                           weak_ptr_factory_.GetWeakPtr(), change_guid, id));
}

bool SyncedNetworkUpdaterImpl::IsUpdateInProgress(
    const std::string& network_guid) {
  return network_guid_to_updates_counter_.contains(network_guid) &&
         network_guid_to_updates_counter_[network_guid] > 0;
}

network_config::mojom::NetworkStatePropertiesPtr
SyncedNetworkUpdaterImpl::FindMojoNetwork(const NetworkIdentifier& id) {
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
          /*limit=*/0),
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnGetNetworkList,
                     base::Unretained(this)));
}

void SyncedNetworkUpdaterImpl::OnGetNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  networks_ = std::move(networks);
}

void SyncedNetworkUpdaterImpl::OnConfigureNetworkResult(
    const std::string& change_guid,
    const sync_pb::WifiConfigurationSpecifics& proto,
    const std::optional<std::string>& network_guid,
    const std::string& error_message) {
  auto id = NetworkIdentifier::FromProto(proto);
  if (network_guid) {
    NET_LOG(EVENT) << "Successfully configured network "
                   << NetworkGuidId(*network_guid);
    NetworkMetadataStore* metadata_store =
        NetworkHandler::Get()->network_metadata_store();
    metadata_store->SetIsConfiguredBySync(*network_guid);
    metadata_store->SetLastConnectedTimestamp(
        *network_guid, base::Milliseconds(proto.last_connected_timestamp()));
  } else {
    NET_LOG(ERROR) << "Failed to configure network "
                   << NetworkId(NetworkStateFromNetworkIdentifier(id))
                   << " because: " << error_message;
    metrics_logger_->RecordApplyNetworkFailureReason(
        ApplyNetworkFailureReason::kFailedToAdd, error_message);
  }
  HandleShillResult(change_guid, id, network_guid.has_value());
}

void SyncedNetworkUpdaterImpl::OnSetPropertiesResult(
    const std::string& change_guid,
    const std::string& network_guid,
    const sync_pb::WifiConfigurationSpecifics& proto,
    bool is_success,
    const std::string& error_message) {
  if (is_success) {
    NET_LOG(EVENT) << "Successfully updated network  "
                   << NetworkGuidId(network_guid);
    NetworkMetadataStore* metadata_store =
        NetworkHandler::Get()->network_metadata_store();
    metadata_store->SetIsConfiguredBySync(network_guid);
    metadata_store->SetLastConnectedTimestamp(
        network_guid, base::Milliseconds(proto.last_connected_timestamp()));
  } else {
    NET_LOG(ERROR) << "Failed to update network "
                   << NetworkGuidId(network_guid);
    metrics_logger_->RecordApplyNetworkFailureReason(
        ApplyNetworkFailureReason::kFailedToUpdate, error_message);
  }
  if (network_guid_to_updates_counter_.contains(network_guid))
    network_guid_to_updates_counter_[network_guid]--;
  HandleShillResult(change_guid, NetworkIdentifier::FromProto(proto),
                    is_success);
}

void SyncedNetworkUpdaterImpl::OnForgetNetworkResult(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    bool is_success) {
  if (is_success) {
    NET_LOG(EVENT) << "Successfully deleted network for change " << change_guid;
  } else {
    NET_LOG(ERROR) << "Failed to remove network for change " << change_guid;
    metrics_logger_->RecordApplyNetworkFailureReason(
        ApplyNetworkFailureReason::kFailedToRemove, "");
  }

  HandleShillResult(change_guid, id, is_success);
}

void SyncedNetworkUpdaterImpl::HandleShillResult(const std::string& change_guid,
                                                 const NetworkIdentifier& id,
                                                 bool is_success) {
  change_guid_to_timer_map_.erase(change_guid);

  if (!tracker_->GetPendingUpdate(change_guid, id)) {
    NET_LOG(EVENT)
        << "Update to network with change_guid " << change_guid
        << " is no longer pending.  This is likely because the change was"
           " preempted by another update to the same network.";
    return;
  }

  if (is_success) {
    tracker_->MarkComplete(change_guid, id);
    metrics_logger_->RecordApplyNetworkSuccess();
    return;
  }

  tracker_->IncrementCompletedAttempts(change_guid, id);
  std::optional<PendingNetworkConfigurationUpdate> update =
      tracker_->GetPendingUpdate(change_guid, id);

  if (update->completed_attempts() >= kMaxRetries) {
    NET_LOG(ERROR) << "Ran out of retries for change " << change_guid
                   << " to network "
                   << NetworkId(NetworkStateFromNetworkIdentifier(id));
    tracker_->MarkComplete(change_guid, id);
    metrics_logger_->RecordApplyNetworkFailed();
    return;
  }

  Retry(*update);
}

void SyncedNetworkUpdaterImpl::CleanupUpdate(const std::string& change_guid,
                                             const NetworkIdentifier& id) {
  tracker_->MarkComplete(change_guid, id);
}

void SyncedNetworkUpdaterImpl::Retry(
    const PendingNetworkConfigurationUpdate& update) {
  if (update.IsDeleteOperation()) {
    network_config::mojom::NetworkStatePropertiesPtr network =
        FindMojoNetwork(update.id());
    if (!network) {
      tracker_->MarkComplete(update.change_guid(), update.id());
      return;
    }

    StartDeleteOperation(update.change_guid(), update.id(), network->guid);
    return;
  }

  StartAddOrUpdateOperation(update.change_guid(), update.id(),
                            update.specifics().value());
}

void SyncedNetworkUpdaterImpl::StartTimer(const std::string& change_guid,
                                          const NetworkIdentifier& id) {
  change_guid_to_timer_map_[change_guid] = timer_factory_->CreateOneShotTimer();
  change_guid_to_timer_map_[change_guid]->Start(
      FROM_HERE, kTimeout,
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnTimeout,
                     base::Unretained(this), change_guid, id));
}

void SyncedNetworkUpdaterImpl::OnTimeout(const std::string& change_guid,
                                         const NetworkIdentifier& id) {
  NET_LOG(ERROR) << "Failed to update network, operation timed out.";
  metrics_logger_->RecordApplyNetworkFailureReason(
      ApplyNetworkFailureReason::kTimedout, "");
  HandleShillResult(change_guid, id, /*is_success=*/false);
}

}  // namespace ash::sync_wifi
