// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

// static
void ManagedCellularPrefHandler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kManagedCellularIccidSmdpPair);
  registry->RegisterDictionaryPref(prefs::kApnMigratedIccids);
}

ManagedCellularPrefHandler::ManagedCellularPrefHandler() = default;
ManagedCellularPrefHandler::~ManagedCellularPrefHandler() = default;

void ManagedCellularPrefHandler::Init(
    NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
}

void ManagedCellularPrefHandler::SetDevicePrefs(PrefService* device_prefs) {
  device_prefs_ = device_prefs;
}

void ManagedCellularPrefHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ManagedCellularPrefHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ManagedCellularPrefHandler::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

void ManagedCellularPrefHandler::NotifyManagedCellularPrefChanged() {
  for (auto& observer : observer_list_)
    observer.OnManagedCellularPrefChanged();
}

void ManagedCellularPrefHandler::AddIccidSmdpPair(
    const std::string& iccid,
    const std::string& smdp_address) {
  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet.";
    return;
  }
  const std::string* existed_smdp_address = GetSmdpAddressFromIccid(iccid);
  if (existed_smdp_address && *existed_smdp_address == smdp_address)
    return;

  NET_LOG(EVENT) << "Adding iccid smdp pair to device pref, iccid: " << iccid
                 << ", smdp: " << smdp_address;
  ScopedDictPrefUpdate update(device_prefs_,
                              prefs::kManagedCellularIccidSmdpPair);
  update->SetByDottedPath(iccid, smdp_address);
  network_state_handler_->SyncStubCellularNetworks();
  NotifyManagedCellularPrefChanged();
}

void ManagedCellularPrefHandler::RemovePairWithIccid(const std::string& iccid) {
  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet.";
    return;
  }
  const std::string* existed_smdp_address = GetSmdpAddressFromIccid(iccid);
  if (!existed_smdp_address)
    return;

  NET_LOG(EVENT) << "Removing iccid smdp pair from device pref, iccid: "
                 << iccid;
  ScopedDictPrefUpdate update(device_prefs_,
                              prefs::kManagedCellularIccidSmdpPair);
  update->RemoveByDottedPath(iccid);
  network_state_handler_->SyncStubCellularNetworks();
  NotifyManagedCellularPrefChanged();
}

const std::string* ManagedCellularPrefHandler::GetSmdpAddressFromIccid(
    const std::string& iccid) const {
  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet.";
    return nullptr;
  }
  const base::Value::Dict& iccid_smdp_pairs =
      device_prefs_->GetDict(prefs::kManagedCellularIccidSmdpPair);
  return iccid_smdp_pairs.FindString(iccid);
}

void ManagedCellularPrefHandler::AddApnMigratedIccid(const std::string& iccid) {
  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet.";
    return;
  }
  if (ContainsApnMigratedIccid(iccid)) {
    NET_LOG(ERROR)
        << "AddApnMigratedIccid: Called with already migrated network, iccid: "
        << iccid;
    return;
  }

  NET_LOG(EVENT)
      << "AddApnMigratedIccid: Adding migrated network to device pref, iccid: "
      << iccid;
  ScopedDictPrefUpdate update(device_prefs_, prefs::kApnMigratedIccids);
  update->SetByDottedPath(iccid, true);
  network_state_handler_->SyncStubCellularNetworks();
  NotifyManagedCellularPrefChanged();
}

bool ManagedCellularPrefHandler::ContainsApnMigratedIccid(
    const std::string& iccid) const {
  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet.";
    return false;
  }
  const base::Value::Dict& apn_migrated_iccids =
      device_prefs_->GetDict(prefs::kApnMigratedIccids);
  return apn_migrated_iccids.FindBool(iccid).value_or(false);
}

}  // namespace ash
