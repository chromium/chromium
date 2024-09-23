// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"

#include <sstream>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {
namespace {

base::flat_set<std::string> GetEuiccPathsFromHermes() {
  base::flat_set<std::string> paths;
  for (const dbus::ObjectPath& euicc_path :
       HermesManagerClient::Get()->GetAvailableEuiccs()) {
    paths.insert(euicc_path.value());
  }
  return paths;
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

void CellularESimProfileHandlerImpl::DeviceListChanged() {
  if (!device_prefs_)
    return;

  AutoRefreshEuiccsIfNecessary();
}

void CellularESimProfileHandlerImpl::InitInternal() {
  network_state_handler_observer_.Observe(network_state_handler());
}

std::vector<CellularESimProfile>
CellularESimProfileHandlerImpl::GetESimProfiles() {
  // Profiles are stored in prefs.
  if (!device_prefs_)
    return std::vector<CellularESimProfile>();

  const base::Value::List& profiles_list =
      device_prefs_->GetList(prefs::kESimProfiles);

  std::vector<CellularESimProfile> profiles;
  for (const base::Value& value : profiles_list) {
    if (!value.is_dict()) {
      NET_LOG(ERROR) << "List item from eSIM profiles pref is not a dictionary";
      continue;
    }

    std::optional<CellularESimProfile> profile =
        CellularESimProfile::FromDictionaryValue(value.GetDict());
    if (!profile) {
      NET_LOG(ERROR) << "Unable to deserialize eSIM profile: " << value;
      continue;
    }

    profiles.push_back(*profile);
  }
  return profiles;
}

bool CellularESimProfileHandlerImpl::HasRefreshedProfilesForEuicc(
    const std::string& eid) {
  base::flat_set<std::string> euicc_paths =
      GetAutoRefreshedEuiccPathsFromPrefs();

  for (const auto& path : euicc_paths) {
    HermesEuiccClient::Properties* euicc_properties =
        HermesEuiccClient::Get()->GetProperties(dbus::ObjectPath(path));
    if (!euicc_properties)
      continue;

    if (eid == euicc_properties->eid().value())
      return true;
  }

  return false;
}

bool CellularESimProfileHandlerImpl::HasRefreshedProfilesForEuicc(
    const dbus::ObjectPath& euicc_path) {
  base::flat_set<std::string> euicc_paths =
      GetAutoRefreshedEuiccPathsFromPrefs();

  for (const auto& path : euicc_paths) {
    if (euicc_path.value() == path)
      return true;
  }

  return false;
}

void CellularESimProfileHandlerImpl::SetDevicePrefs(PrefService* device_prefs) {
  device_prefs_ = device_prefs;
  OnHermesPropertiesUpdated();
}

void CellularESimProfileHandlerImpl::OnHermesPropertiesUpdated() {
  if (!device_prefs_)
    return;

  AutoRefreshEuiccsIfNecessary();
  UpdateProfilesFromHermes();
}

void CellularESimProfileHandlerImpl::AutoRefreshEuiccsIfNecessary() {
  if (!CellularDeviceExists())
    return;

  base::flat_set<std::string> euicc_paths_from_hermes =
      GetEuiccPathsFromHermes();
  base::flat_set<std::string> auto_refreshed_euicc_paths =
      GetAutoRefreshedEuiccPaths();

  base::flat_set<std::string> paths_needing_auto_refresh;
  for (const auto& euicc_path : euicc_paths_from_hermes) {
    if (!base::Contains(auto_refreshed_euicc_paths, euicc_path))
      paths_needing_auto_refresh.insert(euicc_path);
  }

  // We only need to request profiles if we see a new EUICC from Hermes that we
  // have not yet seen before. If no such EUICCs exist, return early.
  if (paths_needing_auto_refresh.empty())
    return;

  StartAutoRefresh(paths_needing_auto_refresh);
}

void CellularESimProfileHandlerImpl::StartAutoRefresh(
    const base::flat_set<std::string>& euicc_paths) {
  paths_pending_auto_refresh_.insert(euicc_paths.begin(), euicc_paths.end());

  // If there is more than one EUICC, log an error. This configuration is not
  // officially supported, so this log may be helpful in feedback reports.
  if (euicc_paths.size() > 1u)
    NET_LOG(ERROR) << "Attempting to refresh profiles from multiple EUICCs";

  // Refresh profiles from the unknown EUICCs. Note that this will internally
  // start an inhibit operation, temporarily blocking the user from changing
  // cellular settings. This operation is only expected to occur when the device
  // originally boots or after a powerwash.
  for (const auto& path : euicc_paths) {
    NET_LOG(EVENT) << "Found new EUICC whose profiles have not yet been "
                   << "refreshed. Refreshing profile list for " << path;
    RefreshProfileListAndRestoreSlot(
        dbus::ObjectPath(path),
        base::BindOnce(
            &CellularESimProfileHandlerImpl::OnAutoRefreshEuiccComplete,
            weak_ptr_factory_.GetWeakPtr(), path));
  }
}

base::flat_set<std::string>
CellularESimProfileHandlerImpl::GetAutoRefreshedEuiccPaths() const {
  // Add all paths stored in prefs.
  base::flat_set<std::string> euicc_paths =
      GetAutoRefreshedEuiccPathsFromPrefs();

  // Add paths which are currently pending a refresh.
  euicc_paths.insert(paths_pending_auto_refresh_.begin(),
                     paths_pending_auto_refresh_.end());

  return euicc_paths;
}

base::flat_set<std::string>
CellularESimProfileHandlerImpl::GetAutoRefreshedEuiccPathsFromPrefs() const {
  DCHECK(device_prefs_);

  const base::Value::List& euicc_paths_from_prefs =
      device_prefs_->GetList(prefs::kESimRefreshedEuiccs);

  base::flat_set<std::string> euicc_paths;
  for (const auto& euicc : euicc_paths_from_prefs) {
    if (!euicc.is_string()) {
      NET_LOG(ERROR) << "Non-string EUICC path: " << euicc;
      continue;
    }
    euicc_paths.insert(euicc.GetString());
  }
  return euicc_paths;
}

void CellularESimProfileHandlerImpl::OnAutoRefreshEuiccComplete(
    const std::string& path,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  paths_pending_auto_refresh_.erase(path);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Auto-refresh failed due to inhibit error. Path: "
                   << path;
    return;
  }

  NET_LOG(EVENT) << "Finished auto-refresh for path: " << path;
  AddNewlyRefreshedEuiccPathToPrefs(path);
}

void CellularESimProfileHandlerImpl::AddNewlyRefreshedEuiccPathToPrefs(
    const std::string& new_path) {
  DCHECK(device_prefs_);

  base::flat_set<std::string> auto_refreshed_euicc_paths =
      GetAutoRefreshedEuiccPathsFromPrefs();

  // Keep all paths which were already in prefs.
  base::Value::List euicc_paths;
  for (const auto& path : auto_refreshed_euicc_paths)
    euicc_paths.Append(path);

  // Add new path.
  euicc_paths.Append(new_path);

  device_prefs_->SetList(prefs::kESimRefreshedEuiccs, std::move(euicc_paths));
}

void CellularESimProfileHandlerImpl::UpdateProfilesFromHermes() {
  DCHECK(device_prefs_);

  std::vector<CellularESimProfile> profiles_from_hermes =
      cellular_utils::GenerateProfilesFromHermes();

  // Don't include pending profiles in the list to cache since we do not provide
  // a mechanism for installing a pending profile except through the dedicated
  // dialog which performs a fresh SM-DS scan each time it is opened.
  std::erase_if(profiles_from_hermes, [](const CellularESimProfile& profile) {
    if (profile.state() == CellularESimProfile::State::kPending) {
      NET_LOG(DEBUG) << "Removing eSIM profile {iccid: " << profile.iccid()
                     << ", eid: " << profile.eid()
                     << "} from list to cache since it is pending";
      return true;
    }
    return false;
  });

  // Skip updating if there are profiles that haven't received ICCID updates
  // yet. This is required because property updates to eSIM profile objects
  // occur after the profile list has been updated. This state is temporary.
  // This method will be triggered again when ICCID properties are updated.
  if (base::ranges::any_of(profiles_from_hermes, &std::string::empty,
                           &CellularESimProfile::iccid)) {
    return;
  }

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
  if (profiles_from_hermes == profiles_before_fetch) {
    return;
  }

  std::stringstream ss;
  ss << "New set of eSIM profiles have been fetched from Hermes: ";

  // Store the updated list of profiles in prefs.
  base::Value::List list;
  for (const auto& profile : profiles_from_hermes) {
    list.Append(profile.ToDictionaryValue());
    ss << "{iccid: " << profile.iccid() << ", eid: " << profile.eid() << "}, ";
  }
  device_prefs_->SetList(prefs::kESimProfiles, std::move(list));

  if (profiles_from_hermes.empty())
    ss << "<empty>";
  NET_LOG(EVENT) << ss.str();

  network_state_handler()->SyncStubCellularNetworks();
  NotifyESimProfileListUpdated();
}

bool CellularESimProfileHandlerImpl::CellularDeviceExists() const {
  return network_state_handler()->GetDeviceStateByType(
             NetworkTypePattern::Cellular()) != nullptr;
}

void CellularESimProfileHandlerImpl::ResetESimProfileCache() {
  DCHECK(device_prefs_);

  device_prefs_->Set(prefs::kESimProfiles,
                     base::Value(base::Value::Type::LIST));
  device_prefs_->Set(prefs::kESimRefreshedEuiccs,
                     base::Value(base::Value::Type::LIST));

  NET_LOG(EVENT) << "Resetting eSIM profile cache";
  OnHermesPropertiesUpdated();
}

void CellularESimProfileHandlerImpl::DisableActiveESimProfile() {
  std::vector<CellularESimProfile> esim_profiles = GetESimProfiles();
  const auto iter =
      base::ranges::find(esim_profiles, CellularESimProfile::State::kActive,
                         &CellularESimProfile::state);
  if (iter == esim_profiles.end()) {
    NET_LOG(EVENT) << "No active eSIM profile is found.";
    return;
  }

  NET_LOG(EVENT) << "Start disabling eSIM profile on path: "
                 << iter->path().value();
  cellular_inhibitor()->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kDisablingProfile,
      base::BindOnce(&CellularESimProfileHandlerImpl::PerformDisableProfile,
                     weak_ptr_factory_.GetWeakPtr(), iter->path()));
}

void CellularESimProfileHandlerImpl::PerformDisableProfile(
    const dbus::ObjectPath& profile_path,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  HermesProfileClient::Get()->DisableCarrierProfile(
      profile_path,
      base::BindOnce(&CellularESimProfileHandlerImpl::OnProfileDisabled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(inhibit_lock)));
}

void CellularESimProfileHandlerImpl::OnProfileDisabled(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "ESimProfile disable error.";
  }
  hermes_metrics::LogDisableProfileResult(status);
}

}  // namespace ash
