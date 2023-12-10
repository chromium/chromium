// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/fake_local_network_collector.h"

#include "base/functional/callback.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace ash::sync_wifi {

FakeLocalNetworkCollector::FakeLocalNetworkCollector() = default;

FakeLocalNetworkCollector::~FakeLocalNetworkCollector() = default;

void FakeLocalNetworkCollector::GetAllSyncableNetworks(
    ProtoListCallback callback) {
  std::move(callback).Run(networks_);
}

void FakeLocalNetworkCollector::GetSyncableNetwork(const std::string& guid,
                                                   ProtoCallback callback) {
  for (sync_pb::WifiConfigurationSpecifics proto : networks_) {
    if (NetworkIdentifier::FromProto(proto).SerializeToString() == guid) {
      std::move(callback).Run(proto);
      return;
    }
  }

  std::move(callback).Run(std::nullopt);
}

std::optional<NetworkIdentifier>
FakeLocalNetworkCollector::GetNetworkIdentifierFromGuid(
    const std::string& guid) {
  for (sync_pb::WifiConfigurationSpecifics proto : networks_) {
    auto id = NetworkIdentifier::FromProto(proto);
    if (id.SerializeToString() == guid) {
      return id;
    }
  }
  return std::nullopt;
}

void FakeLocalNetworkCollector::AddNetwork(
    sync_pb::WifiConfigurationSpecifics proto) {
  networks_.push_back(proto);
}

void FakeLocalNetworkCollector::ClearNetworks() {
  networks_.clear();
}

void FakeLocalNetworkCollector::SetNetworkMetadataStore(
    base::WeakPtr<NetworkMetadataStore> network_metadata_store) {}

void FakeLocalNetworkCollector::FixAutoconnect(
    std::vector<sync_pb::WifiConfigurationSpecifics> protos,
    base::OnceCallback<void()> success_callback) {
  has_fixed_autoconnect_ = true;
  std::move(success_callback).Run();
}

void FakeLocalNetworkCollector::ExecuteAfterNetworksLoaded(
    base::OnceCallback<void()> callback) {
  std::move(callback).Run();
}

}  // namespace ash::sync_wifi
