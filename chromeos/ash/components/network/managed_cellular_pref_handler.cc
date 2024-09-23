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
namespace {
constexpr char kESimMetadataPolicyMissingKey[] = "PolicyMissing";
}  // namespace

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

  if (!device_prefs_ ||
      device_prefs_->HasPrefPath(prefs::kManagedCellularESimMetadata)) {
    return;
  }

  MigrateExistingPrefs();
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

void ManagedCellularPrefHandler::AddESimMetadata(
    const std::string& iccid,
    const std::string& name,
    const policy_util::SmdxActivationCode& activation_code,
    bool sync_stub_networks) {
  DCHECK(!name.empty());
  DCHECK(!activation_code.value().empty());

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
               activation_code.value())
          .Set(kESimMetadataPolicyMissingKey, false);

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
  if (!device_prefs_) {
    NET_LOG(ERROR) << "Device pref not available yet";
    return nullptr;
  }
  const base::Value::Dict& esim_metadata =
      device_prefs_->GetDict(prefs::kManagedCellularESimMetadata);
  return esim_metadata.FindDict(iccid);
}

void ManagedCellularPrefHandler::RemoveESimMetadata(const std::string& iccid) {
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

bool ManagedCellularPrefHandler::IsESimManaged(const std::string& iccid) {
  const base::Value::Dict* esim_metadata = GetESimMetadata(iccid);
  if (!esim_metadata) {
    return false;
  }
  std::optional<bool> policy_missing =
      esim_metadata->FindBool(kESimMetadataPolicyMissingKey);
  // The eSIM is considered managed if the |kESimMetadataPolicyMissingKey| is
  // missing or if the value associated with the key is |false|. This key may be
  // missing since it was added after metadata initially was. For more
  // information see b/361421631.
  if (!policy_missing.has_value()) {
    return true;
  }
  return !policy_missing.value();
}

void ManagedCellularPrefHandler::SetPolicyMissing(const std::string& iccid) {
  const base::Value::Dict* existing_esim_metadata = GetESimMetadata(iccid);
  if (!existing_esim_metadata) {
    return;
  }

  base::Value::Dict esim_metadata = existing_esim_metadata->Clone();
  esim_metadata.Set(kESimMetadataPolicyMissingKey, true);

  NET_LOG(EVENT) << "Setting the policy missing flag for eSIM metadata in "
                 << "device prefs with ICCID: " << iccid;

  ScopedDictPrefUpdate update(device_prefs_,
                              prefs::kManagedCellularESimMetadata);
  update->Set(iccid, std::move(esim_metadata));
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
