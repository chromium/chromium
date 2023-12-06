// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_LOCAL_NETWORK_COLLECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_LOCAL_NETWORK_COLLECTOR_H_

#include <map>
#include <optional>

#include "chromeos/ash/components/sync_wifi/local_network_collector.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"

namespace ash::sync_wifi {

// Test implementation of LocalNetworkCollector.
class FakeLocalNetworkCollector : public LocalNetworkCollector {
 public:
  FakeLocalNetworkCollector();

  FakeLocalNetworkCollector(const FakeLocalNetworkCollector&) = delete;
  FakeLocalNetworkCollector& operator=(const FakeLocalNetworkCollector&) =
      delete;

  ~FakeLocalNetworkCollector() override;

  // sync_wifi::LocalNetworkCollector::
  void GetAllSyncableNetworks(ProtoListCallback callback) override;
  // For test purposes, |guid| == serialized NetworkIdentifier.
  void GetSyncableNetwork(const std::string& guid,
                          ProtoCallback callback) override;
  void RecordZeroNetworksEligibleForSync() override {}
  // For test purposes, |guid| == serialized NetworkIdentifier.
  std::optional<NetworkIdentifier> GetNetworkIdentifierFromGuid(
      const std::string& guid) override;

  void AddNetwork(sync_pb::WifiConfigurationSpecifics proto);
  void ClearNetworks();
  void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store) override;
  void FixAutoconnect(std::vector<sync_pb::WifiConfigurationSpecifics> protos,
                      base::OnceCallback<void()> success_callback) override;
  void ExecuteAfterNetworksLoaded(base::OnceCallback<void()> callback) override;

  bool has_fixed_autoconnect() { return has_fixed_autoconnect_; }

 private:
  std::vector<sync_pb::WifiConfigurationSpecifics> networks_;
  bool has_fixed_autoconnect_ = false;
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_LOCAL_NETWORK_COLLECTOR_H_
