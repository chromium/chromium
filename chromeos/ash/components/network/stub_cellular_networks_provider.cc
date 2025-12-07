// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/stub_cellular_networks_provider.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/uuid.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"

namespace ash {
namespace {

void GetIccids(const NetworkStateHandler::ManagedStateList& network_list,
               base::flat_set<std::string>* all_iccids,
               base::flat_set<std::string>* shill_iccids) {
  for (const std::unique_ptr<ManagedState>& managed_state : network_list) {
    const NetworkState* network = managed_state->AsNetworkState();

    // Skip networks that have not received any property updates yet.
    if (!network->update_received())
      continue;

    // Only cellular networks have ICCIDs.
    if (!NetworkTypePattern::Cellular().MatchesType(network->type()))
      continue;

    std::string iccid = network->iccid();
    if (iccid.empty()) {
      NET_LOG(ERROR) << "Cellular network missing ICCID";
      continue;
    }

    all_iccids->insert(network->iccid());

    if (!network->IsNonProfileType())
      shill_iccids->insert(network->iccid());
  }
}

}  // namespace

StubCellularNetworksProvider::StubCellularNetworksProvider() = default;

StubCellularNetworksProvider::~StubCellularNetworksProvider() {
  network_state_handler_->set_stub_cellular_networks_provider(nullptr);
}

void StubCellularNetworksProvider::Init(
    NetworkStateHandler* network_state_handler,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    ManagedCellularPrefHandler* managed_cellular_pref_handler) {
  network_state_handler_ = network_state_handler;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  managed_cellular_pref_handler_ = managed_cellular_pref_handler;
  network_state_handler_->set_stub_cellular_networks_provider(this);
  network_state_handler_->SyncStubCellularNetworks();
}

bool StubCellularNetworksProvider::AddOrRemoveStubCellularNetworks(
    NetworkStateHandler::ManagedStateList& network_list,
    NetworkStateHandler::ManagedStateList& new_stub_networks,
    const DeviceState* cellular_device) {
  // Remove any existing stub networks if there is no cellular device or
  // cellular technology is not enabled.
  if (!cellular_device || !network_state_handler_->IsTechnologyEnabled(
                              NetworkTypePattern::Cellular())) {
    return RemoveCellularNetworks(/*esim_and_slot_metadata=*/nullptr,
                                  /*shill_iccids=*/nullptr, network_list);
  }

  base::flat_set<std::string> all_iccids, shill_iccids;
  GetIccids(network_list, &all_iccids, &shill_iccids);

  std::vector<IccidEidPair> esim_and_slot_metadata =
      GetESimAndSlotMetadata(cellular_device);

  bool network_list_changed = false;
  network_list_changed |= AddStubNetworks(
      cellular_device, esim_and_slot_metadata, all_iccids, new_stub_networks);
  network_list_changed |= RemoveCellularNetworks(&esim_and_slot_metadata,
                                                 &shill_iccids, network_list);
  network_list_changed |= UpdateCellularNetworks(network_list);

  return network_list_changed;
}

bool StubCellularNetworksProvider::GetStubNetworkMetadata(
    const std::string& iccid,
    const DeviceState* cellular_device,
    std::string* service_path_out,
    std::string* guid_out) {
  std::vector<IccidEidPair> metadata_list =
      GetESimAndSlotMetadata(cellular_device);

  for (const auto& iccid_eid_pair : metadata_list) {
    if (iccid_eid_pair.first != iccid)
      continue;

    *service_path_out = cellular_utils::GenerateStubCellularServicePath(iccid);
    *guid_out = GetGuidForStubIccid(iccid);
    return true;
  }

  return false;
}

const std::string& StubCellularNetworksProvider::GetGuidForStubIccid(
    const std::string& iccid) {
  std::string& guid = iccid_to_guid_map_[iccid];

  // If we have not yet generated a GUID for this ICCID, generate one.
  if (guid.empty())
    guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  return guid;
}

std::vector<StubCellularNetworksProvider::IccidEidPair>
StubCellularNetworksProvider::GetESimAndSlotMetadata(
    const DeviceState* cellular_device) {
  std::vector<IccidEidPair> metadata_list;

  // First, iterate through eSIM profiles and add metadata for installed
  // profiles.
  for (const auto& esim_profile :
       cellular_esim_profile_handler_->GetESimProfiles()) {
    // Skip pending and installing profiles since these are not connectable
    // networks.
    if (esim_profile.state() == CellularESimProfile::State::kInstalling ||
        esim_profile.state() == CellularESimProfile::State::kPending) {
      continue;
    }

    metadata_list.emplace_back(esim_profile.iccid(), esim_profile.eid());
  }

  // Now, iterate through SIM slots and add metadata for pSIM networks.
  for (const CellularSIMSlotInfo& sim_slot_info :
       cellular_device->GetSimSlotInfos()) {
    // Skip empty SIM slots.
    if (sim_slot_info.iccid.empty())
      continue;

    // Skip eSIM slots (which have associated EIDs), since these were already
    // added above.
    if (!sim_slot_info.eid.empty())
      continue;

    metadata_list.emplace_back(sim_slot_info.iccid, /*eid=*/std::string());
  }

  return metadata_list;
}

bool StubCellularNetworksProvider::AddStubNetworks(
    const DeviceState* cellular_device,
    const std::vector<IccidEidPair>& esim_and_slot_metadata,
    const base::flat_set<std::string>& all_iccids,
    NetworkStateHandler::ManagedStateList& new_stub_networks) {
  bool network_added = false;

  for (const IccidEidPair& iccid_eid_pair : esim_and_slot_metadata) {
    // Network already exists for this ICCID; no need to add a stub.
    if (base::Contains(all_iccids, iccid_eid_pair.first))
      continue;

    bool is_managed = false;
    if (managed_cellular_pref_handler_) {
      is_managed =
          managed_cellular_pref_handler_->IsESimManaged(iccid_eid_pair.first);
    }
    NET_LOG(EVENT) << "Adding stub cellular network for ICCID="
                   << iccid_eid_pair.first << " EID=" << iccid_eid_pair.second
                   << ", is managed: " << is_managed;
    network_added = true;
    new_stub_networks.push_back(NetworkState::CreateNonShillCellularNetwork(
        iccid_eid_pair.first, iccid_eid_pair.second,
        GetGuidForStubIccid(iccid_eid_pair.first), is_managed,
        cellular_device->path()));
  }

  return network_added;
}

bool StubCellularNetworksProvider::RemoveCellularNetworks(
    const std::vector<IccidEidPair>* esim_and_slot_metadata,
    const base::flat_set<std::string>* shill_iccids,
    NetworkStateHandler::ManagedStateList& network_list) {
  bool network_removed = false;
  const bool remove_all = !esim_and_slot_metadata && !shill_iccids;

  base::flat_set<std::string> esim_and_slot_iccids;
  if (!remove_all) {
    for (const auto& iccid_eid_pair : *esim_and_slot_metadata)
      esim_and_slot_iccids.insert(iccid_eid_pair.first);
  }

  auto it = network_list.begin();
  while (it != network_list.end()) {
    const NetworkState* network = (*it)->AsNetworkState();

    // Shill backed networks are not stubs and thus should not be removed.
    if (!network->IsNonShillCellularNetwork()) {
      ++it;
      continue;
    }

    if (remove_all || shill_iccids->contains(network->iccid()) ||
        !esim_and_slot_iccids.contains(network->iccid())) {
      NET_LOG(EVENT) << "Removing stub cellular network for ICCID="
                     << network->iccid() << " EID=" << network->eid();
      network_removed = true;
      it = network_list.erase(it);
      continue;
    }

    ++it;
  }

  return network_removed;
}

bool StubCellularNetworksProvider::UpdateCellularNetworks(
    NetworkStateHandler::ManagedStateList& network_list) {
  bool network_changed = false;

  for (auto it = network_list.begin(); it != network_list.end(); ++it) {
    const NetworkState* network = (*it)->AsNetworkState();

    // Shill backed networks are not stubs and thus should not be modified.
    if (!network->IsNonShillCellularNetwork()) {
      continue;
    }

    const bool is_managed =
        managed_cellular_pref_handler_
            ? managed_cellular_pref_handler_->IsESimManaged(network->iccid())
            : false;

    // We only want to update the network if we detect that the managed state
    // has changed.
    if (is_managed == network->IsManagedByPolicy()) {
      continue;
    }

    NET_LOG(EVENT) << "Updating managed state of stub cellular network for "
                   << "ICCID=" << network->iccid() << " EID=" << network->eid()
                   << ", is managed: " << is_managed;
    *it = NetworkState::CreateNonShillCellularNetwork(
        network->iccid(), network->eid(), GetGuidForStubIccid(network->iccid()),
        is_managed, network->device_path());
    network_changed = true;
  }

  return network_changed;
}

}  // namespace ash
