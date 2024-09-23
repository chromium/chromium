// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/local_network_collector_impl.h"

#include "base/barrier_closure.h"
#include "base/uuid.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/sync_wifi/network_eligibility_checker.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/network_type_conversions.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_config {
namespace mojom = ::chromeos::network_config::mojom;
}

namespace ash::sync_wifi {

namespace {

dbus::ObjectPath GetServicePathForGuid(const std::string& guid) {
  const NetworkState* state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!state) {
    return dbus::ObjectPath();
  }

  return dbus::ObjectPath(state->path());
}

bool IsAutoconnectUnspecified(
    const sync_pb::WifiConfigurationSpecifics& proto) {
  return !proto.has_automatically_connect() ||
         proto.automatically_connect() ==
             sync_pb::
                 WifiConfigurationSpecifics_AutomaticallyConnectOption_AUTOMATICALLY_CONNECT_UNSPECIFIED;
}

}  // namespace

LocalNetworkCollectorImpl::LocalNetworkCollectorImpl(
    network_config::mojom::CrosNetworkConfig* cros_network_config,
    SyncedNetworkMetricsLogger* metrics_recorder)
    : cros_network_config_(cros_network_config),
      metrics_recorder_(metrics_recorder) {
  cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());

  // Load the current list of networks.
  OnNetworkStateListChanged();
}

LocalNetworkCollectorImpl::~LocalNetworkCollectorImpl() = default;

void LocalNetworkCollectorImpl::GetAllSyncableNetworks(
    ProtoListCallback callback) {
  if (!is_mojo_networks_loaded_) {
    after_networks_are_loaded_callback_queue_.push(
        base::BindOnce(&LocalNetworkCollectorImpl::GetAllSyncableNetworks,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::string request_guid = InitializeRequest();
  request_guid_to_list_callback_[request_guid] = std::move(callback);

  int count = 0;
  for (const network_config::mojom::NetworkStatePropertiesPtr& network :
       mojo_networks_) {
    if (!IsEligible(network)) {
      continue;
    }

    request_guid_to_in_flight_networks_[request_guid].insert(
        NetworkIdentifier::FromMojoNetwork(network));
    StartGetNetworkDetails(network.get(), request_guid);
    count++;
  }

  if (!count) {
    OnRequestFinished(request_guid);
  }
}

void LocalNetworkCollectorImpl::RecordZeroNetworksEligibleForSync() {
  if (has_logged_zero_eligible_networks_metric_) {
    return;
  }

  if (!is_mojo_networks_loaded_) {
    after_networks_are_loaded_callback_queue_.push(base::BindOnce(
        &LocalNetworkCollectorImpl::RecordZeroNetworksEligibleForSync,
        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  base::flat_set<NetworkEligibilityStatus>
      network_eligible_for_sync_status_codes;
  NetworkEligibilityStatus network_eligible_for_sync_status;
  for (const network_config::mojom::NetworkStatePropertiesPtr& network :
       mojo_networks_) {
    if (!network ||
        network->type != network_config::mojom::NetworkType::kWiFi) {
      continue;
    }
    network_eligible_for_sync_status = GetNetworkEligibilityStatus(
        network->guid, network->connectable,
        network->type_state->get_wifi()->hidden_ssid,
        network->type_state->get_wifi()->security, network->source,
        /*log_result=*/false);
    network_eligible_for_sync_status_codes.insert(
        network_eligible_for_sync_status);
  }
  metrics_recorder_->RecordZeroNetworksEligibleForSync(
      network_eligible_for_sync_status_codes);
  has_logged_zero_eligible_networks_metric_ = true;
}

void LocalNetworkCollectorImpl::GetSyncableNetwork(const std::string& guid,
                                                   ProtoCallback callback) {
  const network_config::mojom::NetworkStateProperties* network = nullptr;
  for (const network_config::mojom::NetworkStatePropertiesPtr& n :
       mojo_networks_) {
    if (n->guid == guid) {
      if (IsEligible(n)) {
        network = n.get();
      }

      break;
    }
  }

  if (!network) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::string request_guid = InitializeRequest();
  request_guid_to_single_callback_[request_guid] = std::move(callback);

  StartGetNetworkDetails(network, request_guid);
}

std::optional<NetworkIdentifier>
LocalNetworkCollectorImpl::GetNetworkIdentifierFromGuid(
    const std::string& guid) {
  for (const network_config::mojom::NetworkStatePropertiesPtr& network :
       mojo_networks_) {
    if (network->guid == guid) {
      return NetworkIdentifier::FromMojoNetwork(network);
    }
  }
  return std::nullopt;
}

void LocalNetworkCollectorImpl::SetNetworkMetadataStore(
    base::WeakPtr<NetworkMetadataStore> network_metadata_store) {
  network_metadata_store_ = network_metadata_store;
}

std::string LocalNetworkCollectorImpl::InitializeRequest() {
  std::string request_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  request_guid_to_complete_protos_[request_guid] =
      std::vector<sync_pb::WifiConfigurationSpecifics>();
  request_guid_to_in_flight_networks_[request_guid] =
      base::flat_set<NetworkIdentifier>();
  return request_guid;
}

bool LocalNetworkCollectorImpl::IsEligible(
    const network_config::mojom::NetworkStatePropertiesPtr& network) {
  if (!network || network->type != network_config::mojom::NetworkType::kWiFi) {
    return false;
  }

  const network_config::mojom::WiFiStatePropertiesPtr& wifi_properties =
      network->type_state->get_wifi();
  return IsEligibleForSync(network->guid, network->connectable,
                           wifi_properties->hidden_ssid,
                           wifi_properties->security, network->source,
                           /*log_result=*/true);
}

void LocalNetworkCollectorImpl::StartGetNetworkDetails(
    const network_config::mojom::NetworkStateProperties* network,
    const std::string& request_guid) {
  sync_pb::WifiConfigurationSpecifics proto;
  proto.set_hex_ssid(network->type_state->get_wifi()->hex_ssid);
  proto.set_security_type(
      SecurityTypeProtoFromMojo(network->type_state->get_wifi()->security));
  base::TimeDelta timestamp =
      network_metadata_store_->GetLastConnectedTimestamp(network->guid);
  proto.set_last_connected_timestamp(timestamp.InMilliseconds());
  cros_network_config_->GetManagedProperties(
      network->guid,
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetManagedPropertiesResult,
                     weak_ptr_factory_.GetWeakPtr(), proto, request_guid));
}

void LocalNetworkCollectorImpl::OnGetManagedPropertiesResult(
    sync_pb::WifiConfigurationSpecifics proto,
    const std::string& request_guid,
    network_config::mojom::ManagedPropertiesPtr properties) {
  if (!properties) {
    NET_LOG(ERROR) << "GetManagedProperties failed.";
    OnNetworkFinished(NetworkIdentifier::FromProto(proto), request_guid);
    return;
  }
  proto.set_automatically_connect(AutomaticallyConnectProtoFromMojo(
      properties->type_properties->get_wifi()->auto_connect));
  proto.set_is_preferred(IsPreferredProtoFromMojo(properties->priority));

  // TODO(crbug/1128692): Restore support for the metered property when mojo
  // networks track the "Automatic" state.

  bool is_proxy_modified =
      network_metadata_store_->GetIsFieldExternallyModified(
          properties->guid, shill::kProxyConfigProperty);
  sync_pb::WifiConfigurationSpecifics_ProxyConfiguration proxy_config =
      ProxyConfigurationProtoFromMojo(properties->proxy_settings,
                                      /*is_unspecified=*/is_proxy_modified);
  proto.mutable_proxy_configuration()->CopyFrom(proxy_config);

  bool is_dns_externally_modified =
      network_metadata_store_->GetIsFieldExternallyModified(
          properties->guid, shill::kNameServersProperty);
  if (properties->static_ip_config &&
      properties->static_ip_config->name_servers &&
      (properties->source == network_config::mojom::OncSource::kUser ||
       !is_dns_externally_modified)) {
    proto.set_dns_option(
        sync_pb::WifiConfigurationSpecifics_DnsOption_DNS_OPTION_CUSTOM);
    for (const std::string& nameserver :
         properties->static_ip_config->name_servers->active_value) {
      proto.add_custom_dns(nameserver);
    }
  } else if (properties->source == network_config::mojom::OncSource::kDevice &&
             is_dns_externally_modified) {
    proto.set_dns_option(
        sync_pb::WifiConfigurationSpecifics_DnsOption_DNS_OPTION_UNSPECIFIED);
  } else {
    proto.set_dns_option(
        sync_pb::WifiConfigurationSpecifics_DnsOption_DNS_OPTION_DEFAULT_DHCP);
  }

  ShillServiceClient::Get()->GetWiFiPassphrase(
      GetServicePathForGuid(properties->guid),
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetWiFiPassphraseResult,
                     weak_ptr_factory_.GetWeakPtr(), proto, request_guid),
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetWiFiPassphraseError,
                     weak_ptr_factory_.GetWeakPtr(),
                     NetworkIdentifier::FromProto(proto), request_guid));
}

void LocalNetworkCollectorImpl::OnGetWiFiPassphraseResult(
    sync_pb::WifiConfigurationSpecifics proto,
    const std::string& request_guid,
    const std::string& passphrase) {
  proto.set_passphrase(passphrase);
  NetworkIdentifier id = NetworkIdentifier::FromProto(proto);
  request_guid_to_complete_protos_[request_guid].push_back(std::move(proto));
  OnNetworkFinished(id, request_guid);
}

void LocalNetworkCollectorImpl::OnGetWiFiPassphraseError(
    const NetworkIdentifier& id,
    const std::string& request_guid,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Failed to get passphrase for " << id.SerializeToString()
                 << " Error: " << error_name << " Details: " << error_message;

  OnNetworkFinished(id, request_guid);
}

void LocalNetworkCollectorImpl::OnNetworkFinished(
    const NetworkIdentifier& id,
    const std::string& request_guid) {
  request_guid_to_in_flight_networks_[request_guid].erase(id);

  if (request_guid_to_in_flight_networks_[request_guid].empty()) {
    OnRequestFinished(request_guid);
  }
}

void LocalNetworkCollectorImpl::OnRequestFinished(
    const std::string& request_guid) {
  DCHECK(request_guid_to_in_flight_networks_[request_guid].empty());

  if (request_guid_to_single_callback_[request_guid]) {
    std::vector<sync_pb::WifiConfigurationSpecifics>& list =
        request_guid_to_complete_protos_[request_guid];
    DCHECK(list.size() <= 1);
    std::optional<sync_pb::WifiConfigurationSpecifics> result;
    if (list.size() == 1) {
      result = list[0];
    }
    std::move(request_guid_to_single_callback_[request_guid]).Run(result);
    request_guid_to_single_callback_.erase(request_guid);
  } else {
    ProtoListCallback callback =
        std::move(request_guid_to_list_callback_[request_guid]);
    DCHECK(callback);
    std::move(callback).Run(request_guid_to_complete_protos_[request_guid]);
    request_guid_to_list_callback_.erase(request_guid);
  }

  request_guid_to_complete_protos_.erase(request_guid);
  request_guid_to_in_flight_networks_.erase(request_guid);
}

void LocalNetworkCollectorImpl::OnNetworkStateListChanged() {
  if (!NetworkHandler::Get()
           ->network_state_handler()
           ->IsProfileNetworksLoaded()) {
    is_mojo_networks_loaded_ = false;
    return;
  }

  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kConfigured,
          network_config::mojom::NetworkType::kWiFi,
          /*limit=*/0),
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetNetworkList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalNetworkCollectorImpl::OnGetNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  mojo_networks_ = std::move(networks);
  is_mojo_networks_loaded_ = true;
  while (!after_networks_are_loaded_callback_queue_.empty()) {
    std::move(after_networks_are_loaded_callback_queue_.front()).Run();
    after_networks_are_loaded_callback_queue_.pop();
  }
}

void LocalNetworkCollectorImpl::ExecuteAfterNetworksLoaded(
    base::OnceClosure callback) {
  if (is_mojo_networks_loaded_) {
    std::move(callback).Run();
    return;
  }

  after_networks_are_loaded_callback_queue_.push(std::move(callback));
}

void LocalNetworkCollectorImpl::FixAutoconnect(
    std::vector<sync_pb::WifiConfigurationSpecifics> protos,
    base::OnceClosure callback) {
  std::vector<std::string> guids_to_fix;
  for (const sync_pb::WifiConfigurationSpecifics& proto : protos) {
    // b/180854680 only affected networks with autoconnect unset/unspecified.
    if (!IsAutoconnectUnspecified(proto)) {
      continue;
    }

    network_config::mojom::NetworkStatePropertiesPtr network =
        GetNetworkFromProto(proto);
    if (!network) {
      // A synced network that is shared could have been removed by another user
      continue;
    }

    guids_to_fix.push_back(network->guid);
  }

  if (guids_to_fix.empty()) {
    std::move(callback).Run();
    return;
  }

  fix_autoconnect_callback_ =
      base::BarrierClosure(guids_to_fix.size(), std::move(callback));
  for (const std::string& guid : guids_to_fix) {
    cros_network_config_->GetManagedProperties(
        guid,
        base::BindOnce(&LocalNetworkCollectorImpl::EnableAutoconnectIfDisabled,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void LocalNetworkCollectorImpl::OnFixAutoconnectComplete(
    bool success,
    const std::string& error) {
  if (!fix_autoconnect_callback_.is_null()) {
    fix_autoconnect_callback_.Run();
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

network_config::mojom::NetworkStatePropertiesPtr
LocalNetworkCollectorImpl::GetNetworkFromProto(
    const sync_pb::WifiConfigurationSpecifics& proto) {
  auto id = NetworkIdentifier::FromProto(proto);
  network_config::mojom::NetworkStatePropertiesPtr network;
  for (const network_config::mojom::NetworkStatePropertiesPtr& n :
       mojo_networks_) {
    if (id == NetworkIdentifier::FromMojoNetwork(n)) {
      return n->Clone();
    }
  }
  return nullptr;
}

void LocalNetworkCollectorImpl::EnableAutoconnectIfDisabled(
    network_config::mojom::ManagedPropertiesPtr properties) {
  if (!properties ||
      properties->type != network_config::mojom::NetworkType::kWiFi) {
    OnFixAutoconnectComplete(/*success=*/true, /*error=*/std::string());
    return;
  }
  if (properties->type_properties->get_wifi()->auto_connect &&
      properties->type_properties->get_wifi()->auto_connect->active_value) {
    OnFixAutoconnectComplete(/*success=*/true, /*error=*/std::string());
    return;
  }

  NET_LOG(EVENT) << "Fixing autoconnect for "
                 << NetworkGuidId(properties->guid);
  auto config = network_config::mojom::ConfigProperties::New();
  config->type_config =
      network_config::mojom::NetworkTypeConfigProperties::NewWifi(
          network_config::mojom::WiFiConfigProperties::New());

  config->auto_connect = network_config::mojom::AutoConnectConfig::New(true);
  cros_network_config_->SetProperties(
      properties->guid, std::move(config),
      base::BindOnce(&LocalNetworkCollectorImpl::OnFixAutoconnectComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash::sync_wifi
