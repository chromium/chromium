// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_FAKE_LOCAL_NETWORK_COLLECTOR_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_FAKE_LOCAL_NETWORK_COLLECTOR_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "chromeos/components/sync_wifi/local_network_collector.h"
#include "chromeos/components/sync_wifi/network_identifier.h"

namespace chromeos {

namespace sync_wifi {

// Test implementation of LocalNetworkCollector.
class FakeLocalNetworkCollector : public LocalNetworkCollector {
 public:
  FakeLocalNetworkCollector();
  ~FakeLocalNetworkCollector() override;

  // sync_wifi::LocalNetworkCollector::
  void GetAllSyncableNetworks(ProtoListCallback callback) override;
  // For test purposes, |guid| == serialized NetworkIdentifier.
  void GetSyncableNetwork(const std::string& guid,
                          ProtoCallback callback) override;
  void RecordZeroNetworksEligibleForSync() override {}
  // For test purposes, |guid| == serialized NetworkIdentifier.
  base::Optional<NetworkIdentifier> GetNetworkIdentifierFromGuid(
      const std::string& guid) override;

  void AddNetwork(sync_pb::WifiConfigurationSpecifics proto);
  void ClearNetworks();
  void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store) override;

 private:
  std::vector<sync_pb::WifiConfigurationSpecifics> networks_;

  DISALLOW_COPY_AND_ASSIGN(FakeLocalNetworkCollector);
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_FAKE_LOCAL_NETWORK_COLLECTOR_H_
