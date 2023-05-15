// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/uuid.h"
#include "chromeos/ash/components/network/cellular_utils.h"

namespace ash {

FakeStubCellularNetworksProvider::FakeStubCellularNetworksProvider() = default;

FakeStubCellularNetworksProvider::~FakeStubCellularNetworksProvider() = default;

void FakeStubCellularNetworksProvider::AddStub(const std::string& stub_iccid,
                                               const std::string& stub_eid,
                                               bool is_managed) {
  stub_iccid_and_eid_pairs_.emplace(stub_iccid, stub_eid);
  if (is_managed)
    managed_iccids_.insert(stub_iccid);
}

void FakeStubCellularNetworksProvider::RemoveStub(const std::string& stub_iccid,
                                                  const std::string& stub_eid) {
  stub_iccid_and_eid_pairs_.erase(std::make_pair(stub_iccid, stub_eid));
  managed_iccids_.erase(stub_iccid);
}

bool FakeStubCellularNetworksProvider::AddOrRemoveStubCellularNetworks(
    NetworkStateHandler::ManagedStateList& network_list,
    NetworkStateHandler::ManagedStateList& new_stub_networks,
    const DeviceState* device) {
  bool changed = false;

  // Add new stub networks if they do not correspond to an existing Shill-backed
  // network.
  std::vector<IccidEidPair> stubs_to_add =
      GetStubsNotBackedByShill(network_list);
  if (!stubs_to_add.empty()) {
    changed = true;
    for (const IccidEidPair& pair : stubs_to_add) {
      new_stub_networks.push_back(NetworkState::CreateNonShillCellularNetwork(
          pair.first, pair.second, GetGuidForStubIccid(pair.first),
          base::Contains(managed_iccids_, pair.first), device->path()));
      stub_networks_add_count_++;
    }
  }

  // Removing existing stub networks if they are no longer applicable.
  auto network_list_it = network_list.begin();
  while (network_list_it != network_list.end()) {
    const NetworkState* network = (*network_list_it)->AsNetworkState();

    if (!network->IsNonShillCellularNetwork()) {
      ++network_list_it;
      continue;
    }

    std::string iccid = network->iccid();

    // Stub network which corresponds to a removed stub ICCID; remove.
    if (!base::Contains(stub_iccid_and_eid_pairs_, iccid,
                        &IccidEidPair::first)) {
      changed = true;
      network_list_it = network_list.erase(network_list_it);
      continue;
    }

    if (base::ranges::none_of(
            network_list, [&iccid](const std::unique_ptr<ManagedState>& state) {
              const NetworkState* network = state->AsNetworkState();
              return !network->IsNonShillCellularNetwork() &&
                     network->iccid() == iccid;
            })) {
      ++network_list_it;
      continue;
    }

    // Stub network which has been replaced by a non-stub network; remove.
    changed = true;
    network_list_it = network_list.erase(network_list_it);
  }

  return changed;
}

bool FakeStubCellularNetworksProvider::GetStubNetworkMetadata(
    const std::string& iccid,
    const DeviceState* cellular_device,
    std::string* service_path_out,
    std::string* guid_out) {
  if (!base::Contains(stub_iccid_and_eid_pairs_, iccid, &IccidEidPair::first))
    return false;

  *service_path_out = cellular_utils::GenerateStubCellularServicePath(iccid);
  *guid_out = GetGuidForStubIccid(iccid);
  return true;
}

const std::string& FakeStubCellularNetworksProvider::GetGuidForStubIccid(
    const std::string& iccid) {
  std::string& guid = iccid_to_guid_map_[iccid];

  // If we have not yet generated a GUID for this ICCID, generate one.
  if (guid.empty())
    guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  return guid;
}

std::vector<FakeStubCellularNetworksProvider::IccidEidPair>
FakeStubCellularNetworksProvider::GetStubsNotBackedByShill(
    const NetworkStateHandler::ManagedStateList& network_list) const {
  std::vector<IccidEidPair> not_backed_by_shill;

  for (const IccidEidPair& pair : stub_iccid_and_eid_pairs_) {
    // Only need to add a stub network if the stub ICCID does not match the
    // ICCID of a Shill-backed network.
    if (!base::Contains(network_list, pair.first,
                        [](const std::unique_ptr<ManagedState>& state) {
                          return state->AsNetworkState()->iccid();
                        })) {
      not_backed_by_shill.push_back(pair);
    }
  }

  return not_backed_by_shill;
}

}  // namespace ash
