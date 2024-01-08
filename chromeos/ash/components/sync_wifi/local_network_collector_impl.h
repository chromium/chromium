// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/sync_wifi/local_network_collector.h"
#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace sync_pb {
class WifiConfigurationSpecifics;
}

namespace ash {

class NetworkMetadataStore;

namespace sync_wifi {

// Handles the retrieval, filtering, and conversion of local network
// configurations to syncable protos.  Local networks are retrieved from Shill
// via the cros_network_config mojo interface, and passwords come directly from
// ShillServiceClient.
class LocalNetworkCollectorImpl
    : public LocalNetworkCollector,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  // LocalNetworkCollector:

  // |cros_network_config| and |network_metadata_store| must outlive this class.
  explicit LocalNetworkCollectorImpl(
      chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config,
      SyncedNetworkMetricsLogger* metrics_recorder);
  ~LocalNetworkCollectorImpl() override;

  // Can only execute one request at a time.
  void GetAllSyncableNetworks(ProtoListCallback callback) override;

  // Executes at most once per instance of LocalNetworkCollectorImpl.
  void RecordZeroNetworksEligibleForSync() override;

  // Can be called on multiple networks simultaneously.
  void GetSyncableNetwork(const std::string& guid,
                          ProtoCallback callback) override;

  std::optional<NetworkIdentifier> GetNetworkIdentifierFromGuid(
      const std::string& guid) override;

  void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store) override;

  void FixAutoconnect(std::vector<sync_pb::WifiConfigurationSpecifics> protos,
                      base::OnceClosure callback) override;

  void ExecuteAfterNetworksLoaded(base::OnceClosure callback) override;

  // CrosNetworkConfigObserver:
  void OnNetworkStateListChanged() override;

 private:
  std::string InitializeRequest();
  bool IsEligible(
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
          network);
  void StartGetNetworkDetails(
      const chromeos::network_config::mojom::NetworkStateProperties* network,
      const std::string& request_guid);
  void OnGetManagedPropertiesResult(
      sync_pb::WifiConfigurationSpecifics proto,
      const std::string& request_guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr properties);
  void EnableAutoconnectIfDisabled(
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);

  // Callback for shill's GetWiFiPassphrase method.  |proto| should contain the
  // partially filled in proto representation of the network, |is_one_off|
  // should be true when GetSyncableNetwork is used rather than
  // GetAllSyncableNetworks and |passphrase| will come from shill.
  void OnGetWiFiPassphraseResult(sync_pb::WifiConfigurationSpecifics proto,
                                 const std::string& request_guid,
                                 const std::string& passphrase);

  // An empty |request_guid| implies that this is part of a request for a list
  // while a populated |request_guid| implies a one off network request.
  void OnGetWiFiPassphraseError(const NetworkIdentifier& id,
                                const std::string& request_guid,
                                const std::string& error_name,
                                const std::string& error_message);

  void OnNetworkFinished(const NetworkIdentifier& id,
                         const std::string& request_guid);
  void OnRequestFinished(const std::string& request_guid);
  void OnGetNetworkList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  chromeos::network_config::mojom::NetworkStatePropertiesPtr
  GetNetworkFromProto(const sync_pb::WifiConfigurationSpecifics& proto);
  void OnFixAutoconnectComplete(bool success, const std::string& error);

  raw_ptr<chromeos::network_config::mojom::CrosNetworkConfig, DanglingUntriaged>
      cros_network_config_;
  raw_ptr<SyncedNetworkMetricsLogger, DanglingUntriaged> metrics_recorder_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
      mojo_networks_;
  base::WeakPtr<NetworkMetadataStore> network_metadata_store_;

  base::flat_map<std::string, std::vector<sync_pb::WifiConfigurationSpecifics>>
      request_guid_to_complete_protos_;
  base::flat_map<std::string, base::flat_set<NetworkIdentifier>>
      request_guid_to_in_flight_networks_;
  base::flat_map<std::string, ProtoCallback> request_guid_to_single_callback_;
  base::flat_map<std::string, ProtoListCallback> request_guid_to_list_callback_;
  base::queue<base::OnceClosure> after_networks_are_loaded_callback_queue_;
  bool is_mojo_networks_loaded_ = false;
  bool has_logged_zero_eligible_networks_metric_ = false;

  // This will be null unless there is FixAutoconnect operation in progress.
  base::RepeatingClosure fix_autoconnect_callback_;

  base::WeakPtrFactory<LocalNetworkCollectorImpl> weak_ptr_factory_{this};
};

}  // namespace sync_wifi

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_IMPL_H_
