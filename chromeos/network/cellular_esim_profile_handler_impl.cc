// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_profile_handler_impl.h"

#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/network_event_log.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace {

base::flat_set<std::string> GetEuiccPathsFromHermes() {
  base::flat_set<std::string> paths;
  for (const dbus::ObjectPath& euicc_path :
       HermesManagerClient::Get()->GetAvailableEuiccs()) {
    paths.insert(euicc_path.value());
  }
  return paths;
}

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

// static
void CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kESimRefreshedEuiccs);
  registry->RegisterListPref(prefs::kESimProfiles);
}

CellularESimProfileHandlerImpl::CellularESimProfileHandlerImpl() = default;

CellularESimProfileHandlerImpl::~CellularESimProfileHandlerImpl() = default;

void CellularESimProfileHandlerImpl::InitInternal() {
  network_state_handler()->SyncStubCellularNetworks();
}

std::vector<CellularESimProfile>
CellularESimProfileHandlerImpl::GetESimProfiles() {
  // Profiles are stored in prefs.
  if (!device_prefs_)
    return std::vector<CellularESimProfile>();

  const base::ListValue* profiles_list =
      device_prefs_->GetList(prefs::kESimProfiles);
  if (!profiles_list) {
    NET_LOG(ERROR) << "eSIM profiles pref is not a list";
    return std::vector<CellularESimProfile>();
  }

  std::vector<CellularESimProfile> profiles;
  for (const base::Value& value : profiles_list->GetList()) {
    const base::DictionaryValue* dict;
    if (!value.GetAsDictionary(&dict)) {
      NET_LOG(ERROR) << "List item from eSIM profiles pref is not a dictionary";
      continue;
    }

    base::Optional<CellularESimProfile> profile =
        CellularESimProfile::FromDictionaryValue(*dict);
    if (!profile) {
      NET_LOG(ERROR) << "Unable to deserialize eSIM profile: " << *dict;
      continue;
    }

    profiles.push_back(*profile);
  }
  return profiles;
}

void CellularESimProfileHandlerImpl::SetDevicePrefs(PrefService* device_prefs) {
  device_prefs_ = device_prefs;
  OnHermesPropertiesUpdated();
}

void CellularESimProfileHandlerImpl::OnHermesPropertiesUpdated() {
  if (!device_prefs_)
    return;

  RefreshEuiccsIfNecessary();
  UpdateProfilesFromHermes();
}

void CellularESimProfileHandlerImpl::RefreshEuiccsIfNecessary() {
  base::flat_set<std::string> euicc_paths_from_hermes =
      GetEuiccPathsFromHermes();
  base::flat_set<std::string> euicc_paths_from_prefs = GetEuiccPathsFromPrefs();

  // If the paths in prefs and Hermes match, we have already tried refreshing
  // them both, and there is nothing else to do.
  if (euicc_paths_from_hermes == euicc_paths_from_prefs)
    return;

  base::flat_set<std::string> paths_in_hermes_but_not_prefs;
  for (const auto& hermes_path : euicc_paths_from_hermes) {
    if (!base::Contains(euicc_paths_from_prefs, hermes_path))
      paths_in_hermes_but_not_prefs.insert(hermes_path);
  }

  // We only need to request profiles if we see a new EUICC from Hermes that we
  // have not yet seen before. If no such EUICCs exist, return early.
  if (paths_in_hermes_but_not_prefs.empty())
    return;

  // If there is more than one EUICC, log a warning. This configuration is not
  // officially supported, so this may be helpful in feedback reports.
  if (paths_in_hermes_but_not_prefs.size() > 1u)
    NET_LOG(ERROR) << "Attempting to refresh profiles from multiple EUICCs";

  // Combine both sets together and store them to prefs to ensure that we do not
  // need to refresh again for the same EUICCs.
  base::flat_set<std::string> all_paths;
  all_paths.insert(euicc_paths_from_prefs.begin(),
                   euicc_paths_from_prefs.end());
  all_paths.insert(euicc_paths_from_hermes.begin(),
                   euicc_paths_from_hermes.end());
  StoreEuiccPathsToPrefs(all_paths);

  // Refresh profiles from the unknown EUICCs. Note that this will internally
  // start an inhibit operation, temporarily blocking the user from changing
  // cellular settings. This operation is only expected to occur when the device
  // originally boots or after a powerwash.
  for (const auto& path : paths_in_hermes_but_not_prefs) {
    NET_LOG(EVENT) << "Found new EUICC whose profiles have not yet been "
                   << "refreshsed. Refreshing profile list for " << path;
    RefreshProfileList(dbus::ObjectPath(path), base::DoNothing());
  }
}

base::flat_set<std::string>
CellularESimProfileHandlerImpl::GetEuiccPathsFromPrefs() const {
  const base::ListValue* euicc_paths_from_prefs =
      device_prefs_->GetList(prefs::kESimRefreshedEuiccs);
  if (!euicc_paths_from_prefs) {
    NET_LOG(ERROR) << "Could not fetch refreshed EUICCs pref.";
    return {};
  }

  base::flat_set<std::string> euicc_paths_from_prefs_set;
  for (const auto& euicc : *euicc_paths_from_prefs) {
    if (!euicc.is_string()) {
      NET_LOG(ERROR) << "Non-string EUICC path: " << euicc;
      continue;
    }
    euicc_paths_from_prefs_set.insert(euicc.GetString());
  }
  return euicc_paths_from_prefs_set;
}

void CellularESimProfileHandlerImpl::StoreEuiccPathsToPrefs(
    const base::flat_set<std::string>& paths) {
  DCHECK(device_prefs_);

  base::Value euicc_paths(base::Value::Type::LIST);
  for (const auto& path : paths)
    euicc_paths.Append(path);

  device_prefs_->Set(prefs::kESimRefreshedEuiccs, std::move(euicc_paths));
}

void CellularESimProfileHandlerImpl::UpdateProfilesFromHermes() {
  DCHECK(device_prefs_);

  std::vector<CellularESimProfile> profiles_from_hermes =
      GenerateProfilesFromHermes();

  // When the device starts up, Hermes is expected to return an empty list of
  // profiles if the profiles have not yet been requested. If this occurs,
  // return early and rely on the profiles stored in prefs instead.
  if (profiles_from_hermes.empty() &&
      !has_completed_successful_profile_refresh()) {
    return;
  }

  std::vector<CellularESimProfile> profiles_before_fetch = GetESimProfiles();

  // If nothing has changed since the last update, do not update prefs or notify
  // observers of a change.
  if (profiles_from_hermes == profiles_before_fetch)
    return;

  // Store the updated list of profiles in prefs.
  base::Value list(base::Value::Type::LIST);
  for (const auto& profile : profiles_from_hermes)
    list.Append(profile.ToDictionaryValue());
  device_prefs_->Set(prefs::kESimProfiles, std::move(list));

  network_state_handler()->SyncStubCellularNetworks();
  NotifyESimProfileListUpdated();
}

bool CellularESimProfileHandlerImpl::AddOrRemoveStubCellularNetworks(
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
  std::vector<CellularESimProfile> esim_profiles = GetESimProfiles();
  network_list_changed |=
      AddStubESimNetworks(device, esim_profiles, new_stub_networks, all_iccids);
  network_list_changed |=
      AddStubPSimNetworks(device, new_stub_networks, all_iccids);
  network_list_changed |= RemoveStubCellularNetworks(
      device, esim_profiles, network_list, all_shill_iccids);

  return network_list_changed;
}

}  // namespace chromeos
