// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/stub_cellular_networks_provider.h"

#include "base/containers/flat_set.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/device_state.h"

namespace chromeos {
namespace {

// Adds non-shill stub cellular networks to |new_stub_networks| list for all
// profiles in |esim_profiles| that do not already exists in |all_iccids|.
bool AddStubESimNetworks(
    const DeviceState* device,
    const std::vector<CellularESimProfile>& esim_profiles,
    NetworkStateHandler::ManagedStateList& new_stub_networks,
    const base::flat_set<std::string>& all_iccids) {
  bool network_list_changed = false;
  for (const auto& esim_profile : esim_profiles) {
    // Skip pending and installing profiles since these are not connectable
    // networks.
    if (esim_profile.state() == CellularESimProfile::State::kInstalling ||
        esim_profile.state() == CellularESimProfile::State::kPending) {
      continue;
    }

    if (all_iccids.contains(esim_profile.iccid()))
      continue;

    network_list_changed = true;
    new_stub_networks.push_back(NetworkState::CreateNonShillCellularNetwork(
        esim_profile.iccid(), esim_profile.eid(), device));
  }
  return network_list_changed;
}

// Adds non-shill stub cellular networks to |new_stub_networks| for pSIM ICCIDs
// in the slot info object on given cellular |device| that do not already
// exists in |all_iccids|.
bool AddStubPSimNetworks(
    const DeviceState* device,
    NetworkStateHandler::ManagedStateList& new_stub_networks,
    const base::flat_set<std::string>& all_iccids) {
  bool network_list_changed = false;
  for (const CellularSIMSlotInfo& sim_slot_info : device->sim_slot_infos()) {
    // Skip empty SIM slots.
    if (sim_slot_info.iccid.empty())
      continue;

    // Skip pSIM slots.
    if (!sim_slot_info.eid.empty())
      continue;

    if (all_iccids.contains(sim_slot_info.iccid))
      continue;

    network_list_changed = true;
    new_stub_networks.push_back(NetworkState::CreateNonShillCellularNetwork(
        sim_slot_info.iccid, /*eid=*/std::string(), device));
  }
  return network_list_changed;
}

// Removes all non-shill stub cellular networks from |network_list| that are
// not required anymore viz. 1) Stub networks for which a corresponding entry
// already exists in |all_shill_iccids| 2) Stub networks that do not have
// corresponding entry in |esim_profiles| or a slot info entry on given |device|
// (e.g. eSIM profile was removed or pSIM was removed from the slot).
bool RemoveStubCellularNetworks(
    const DeviceState* device,
    const std::vector<CellularESimProfile>& esim_profiles,
    NetworkStateHandler::ManagedStateList& network_list,
    const base::flat_set<std::string>& all_shill_iccids) {
  base::flat_set<std::string> hermes_and_slot_iccids;
  bool network_list_changed = false;

  for (const auto& esim_profile : esim_profiles) {
    hermes_and_slot_iccids.insert(esim_profile.iccid());
  }

  for (const CellularSIMSlotInfo& sim_slot_info : device->sim_slot_infos()) {
    if (sim_slot_info.iccid.empty())
      continue;
    hermes_and_slot_iccids.insert(sim_slot_info.iccid);
  }

  for (auto iter = network_list.begin(); iter != network_list.end();) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (!network->IsNonShillCellularNetwork()) {
      iter++;
      continue;
    }

    if (all_shill_iccids.contains(network->iccid()) ||
        !hermes_and_slot_iccids.contains(network->iccid())) {
      network_list_changed = true;
      iter = network_list.erase(iter);
    } else {
      iter++;
    }
  }

  return network_list_changed;
}

}  // namespace

StubCellularNetworksProvider::StubCellularNetworksProvider() = default;

StubCellularNetworksProvider::~StubCellularNetworksProvider() {
  network_state_handler_->set_stub_cellular_networks_provider(nullptr);
}

void StubCellularNetworksProvider::Init(
    NetworkStateHandler* network_state_handler,
    CellularESimProfileHandler* cellular_esim_profile_handler) {
  network_state_handler_ = network_state_handler;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  network_state_handler_->set_stub_cellular_networks_provider(this);
  network_state_handler_->SyncStubCellularNetworks();
}

bool StubCellularNetworksProvider::AddOrRemoveStubCellularNetworks(
    NetworkStateHandler::ManagedStateList& network_list,
    NetworkStateHandler::ManagedStateList& new_stub_networks,
    const DeviceState* device) {
  // Do not create any new stub networks if there is no cellular device.
  if (!device)
    return false;

  // Find set of existing ICCIDs.
  base::flat_set<std::string> all_shill_iccids, all_iccids;
  for (std::unique_ptr<ManagedState>& managed_state : network_list) {
    const NetworkState* network = managed_state->AsNetworkState();
    // Ignore non-cellular networks or networks that have not received any
    // property updates yet.
    if (!network->update_received() ||
        !NetworkTypePattern::Cellular().MatchesType(network->type())) {
      continue;
    }

    if (!network->IsNonShillCellularNetwork()) {
      all_shill_iccids.insert(network->iccid());
    }

    all_iccids.insert(network->iccid());
  }

  bool network_list_changed = false;
  std::vector<CellularESimProfile> esim_profiles =
      cellular_esim_profile_handler_->GetESimProfiles();
  network_list_changed |=
      AddStubESimNetworks(device, esim_profiles, new_stub_networks, all_iccids);
  network_list_changed |=
      AddStubPSimNetworks(device, new_stub_networks, all_iccids);
  network_list_changed |= RemoveStubCellularNetworks(
      device, esim_profiles, network_list, all_shill_iccids);

  return network_list_changed;
}

}  // namespace chromeos
