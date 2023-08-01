// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

// static
void ManagedCellularPrefHandler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kManagedCellularESimMetadata);
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

  if (!device_prefs_) {
    return;
  }

  const bool hasPref =
      device_prefs_->HasPrefPath(prefs::kManagedCellularESimMetadata);
  if (!ash::features::IsSmdsSupportEuiccUploadEnabled()) {
    if (hasPref) {
      device_prefs_->ClearPref(prefs::kManagedCellularESimMetadata);
    }
    return;
  }
  if (!hasPref) {
    MigrateExistingPrefs();
  }
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
    const std::string& smdp_address,
    bool sync_stub_networks) {
  DCHECK(!ash::features::IsSmdsSupportEuiccUploadEnabled());

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
  if (sync_stub_networks) {
    network_state_handler_->SyncStubCellularNetworks();
  }

  NotifyManagedCellularPrefChanged();
}

void ManagedCellularPrefHandler::RemovePairWithIccid(const std::string& iccid) {
  DCHECK(!ash::features::IsSmdsSupportEuiccUploadEnabled());

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
  DCHECK(!ash::features::IsSmdsSupportEuiccUploadEnabled());

  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet.";
    return nullptr;
  }
  const base::Value::Dict& iccid_smdp_pairs =
      device_prefs_->GetDict(prefs::kManagedCellularIccidSmdpPair);
  return iccid_smdp_pairs.FindString(iccid);
}

void ManagedCellularPrefHandler::AddESimMetadata(
    const std::string& iccid,
    const std::string& name,
    const policy_util::SmdxActivationCode& activation_code,
    bool sync_stub_networks) {
  DCHECK(ash::features::IsSmdsSupportEuiccUploadEnabled());

  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet";
    return;
  }

  auto esim_metadata =
      base::Value::Dict()
          .Set(::onc::network_config::kName, name)
          .Set(activation_code.type() ==
                       policy_util::SmdxActivationCode::Type::SMDP
                   ? ::onc::cellular::kSMDPAddress
                   : ::onc::cellular::kSMDSAddress,
               activation_code.value());

  const base::Value::Dict* existing_esim_metadata = GetESimMetadata(iccid);
  if (existing_esim_metadata && *existing_esim_metadata == esim_metadata) {
    return;
  }

  NET_LOG(EVENT) << "Adding eSIM metadata to device prefs, ICCID: " << iccid
                 << ", name: " << name
                 << ", activation code: " << activation_code.ToString();

  ScopedDictPrefUpdate update(device_prefs_,
                              prefs::kManagedCellularESimMetadata);
  update->Set(iccid, std::move(esim_metadata));
  if (sync_stub_networks) {
    network_state_handler_->SyncStubCellularNetworks();
  }
  NotifyManagedCellularPrefChanged();
}

const base::Value::Dict* ManagedCellularPrefHandler::GetESimMetadata(
    const std::string& iccid) {
  DCHECK(ash::features::IsSmdsSupportEuiccUploadEnabled());

  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet";
    return nullptr;
  }
  const base::Value::Dict& esim_metadata =
      device_prefs_->GetDict(prefs::kManagedCellularESimMetadata);
  return esim_metadata.FindDict(iccid);
}

void ManagedCellularPrefHandler::RemoveESimMetadata(const std::string& iccid) {
  DCHECK(ash::features::IsSmdsSupportEuiccUploadEnabled());

  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet";
    return;
  }

  const base::Value::Dict& esim_metadata =
      device_prefs_->GetDict(prefs::kManagedCellularESimMetadata);
  if (!esim_metadata.contains(iccid)) {
    return;
  }

  NET_LOG(EVENT) << "Removing eSIM metadata from device prefs, ICCID: "
                 << iccid;
  ScopedDictPrefUpdate update(device_prefs_,
                              prefs::kManagedCellularESimMetadata);
  update->Remove(iccid);
  network_state_handler_->SyncStubCellularNetworks();
  NotifyManagedCellularPrefChanged();
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

void ManagedCellularPrefHandler::MigrateExistingPrefs() {
  DCHECK(ash::features::IsSmdsSupportEuiccUploadEnabled());
  DCHECK(device_prefs_);

  NET_LOG(EVENT) << "Starting migration of existing ICCID and SM-DP+ pairs";

  const base::Value::Dict& existing_prefs =
      device_prefs_->GetDict(prefs::kManagedCellularIccidSmdpPair);

  for (const auto [iccid, value] : existing_prefs) {
    const std::string& smdp_activation_code = value.GetString();
    if (smdp_activation_code.empty()) {
      NET_LOG(ERROR) << "Failed to migrate ICCID and SM-DP+ pair due to "
                     << "missing activation code";
      continue;
    }

    ScopedDictPrefUpdate update(device_prefs_,
                                prefs::kManagedCellularESimMetadata);
    update->Set(iccid, base::Value::Dict().Set(::onc::cellular::kSMDPAddress,
                                               smdp_activation_code));

    NET_LOG(EVENT) << "Successfully migrated ICCID and SM-DP+ pair";
  }

  NET_LOG(EVENT) << "Finished migration of existing ICCID and SM-DP+ pairs";
}

}  // namespace ash
