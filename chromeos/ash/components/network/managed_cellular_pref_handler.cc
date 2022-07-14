// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace chromeos {

// static
void ManagedCellularPrefHandler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(ash::prefs::kManagedCellularIccidSmdpPair);
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
  ::prefs::ScopedDictionaryPrefUpdate update(
      device_prefs_, ash::prefs::kManagedCellularIccidSmdpPair);
  update->SetString(iccid, smdp_address);
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
  ::prefs::ScopedDictionaryPrefUpdate update(
      device_prefs_, ash::prefs::kManagedCellularIccidSmdpPair);
  update->Remove(iccid);
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
      device_prefs_->GetValueDict(ash::prefs::kManagedCellularIccidSmdpPair);
  return iccid_smdp_pairs.FindString(iccid);
}

}  // namespace chromeos
