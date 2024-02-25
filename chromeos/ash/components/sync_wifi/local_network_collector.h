// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace sync_pb {
class WifiConfigurationSpecifics;
}

namespace ash {

class NetworkMetadataStore;

namespace sync_wifi {

// Handles the retrieval, filtering, and conversion of local network
// configurations to syncable protos.
class LocalNetworkCollector {
 public:
  typedef base::OnceCallback<void(
      std::optional<sync_pb::WifiConfigurationSpecifics>)>
      ProtoCallback;

  typedef base::OnceCallback<void(
      std::vector<sync_pb::WifiConfigurationSpecifics>)>
      ProtoListCallback;

  LocalNetworkCollector() = default;
  virtual ~LocalNetworkCollector() = default;

  // Creates a list of all local networks which are syncable and delivers them
  // to the provided |callback|.  This excludes networks which are managed,
  // unsecured, use enterprise security, or shared.
  virtual void GetAllSyncableNetworks(ProtoListCallback callback) = 0;

  // Creates a WifiConfigurationSpecifics proto with the relevant network
  // details for the network with the given |id|.  If that network doesn't
  // exist or isn't syncable it will provide std::nullopt to the callback.
  virtual void GetSyncableNetwork(const std::string& guid,
                                  ProtoCallback callback) = 0;

  // Record the reason(s) why zero of the local networks are eligible to be
  // synced.
  virtual void RecordZeroNetworksEligibleForSync() = 0;

  // Retrieves the NetworkIdentifier for a given local network's |guid|
  // if the network no longer exists it returns nullopt.
  virtual std::optional<NetworkIdentifier> GetNetworkIdentifierFromGuid(
      const std::string& guid) = 0;

  // Provides the metadata store which gets constructed later.
  virtual void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store) = 0;

  // Fixes networks affected by b/180854680, to be removed after M-91.
  virtual void FixAutoconnect(
      std::vector<sync_pb::WifiConfigurationSpecifics> protos,
      base::OnceClosure callback) = 0;

  // Executes the given callback after the local mojo networks have been loaded.
  // If already loaded, the callback will be executed immediately.
  virtual void ExecuteAfterNetworksLoaded(base::OnceClosure callback) = 0;
};

}  // namespace sync_wifi

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_H_
