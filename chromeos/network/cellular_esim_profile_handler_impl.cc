// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_profile_handler_impl.h"

#include <algorithm>
#include <iterator>

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/network_event_log.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {

// static
void CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kESimProfilesPrefName);
}

CellularESimProfileHandlerImpl::CellularESimProfileHandlerImpl() = default;

CellularESimProfileHandlerImpl::~CellularESimProfileHandlerImpl() {
  HermesManagerClient::Get()->RemoveObserver(this);
  HermesEuiccClient::Get()->RemoveObserver(this);
  HermesProfileClient::Get()->RemoveObserver(this);
}

void CellularESimProfileHandlerImpl::Init() {
  HermesManagerClient::Get()->AddObserver(this);
  HermesEuiccClient::Get()->AddObserver(this);
  HermesProfileClient::Get()->AddObserver(this);
}

std::vector<CellularESimProfile>
CellularESimProfileHandlerImpl::GetESimProfiles() {
  // Profiles are stored in prefs.
  if (!device_prefs_)
    return std::vector<CellularESimProfile>();

  const base::ListValue* profiles_list =
      device_prefs_->GetList(prefs::kESimProfilesPrefName);
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
  UpdateProfilesFromHermes();
}

void CellularESimProfileHandlerImpl::OnAvailableEuiccListChanged() {
  UpdateProfilesFromHermes();
}

void CellularESimProfileHandlerImpl::OnEuiccPropertyChanged(
    const dbus::ObjectPath& euicc_path,
    const std::string& property_name) {
  UpdateProfilesFromHermes();
}

void CellularESimProfileHandlerImpl::OnCarrierProfilePropertyChanged(
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& property_name) {
  UpdateProfilesFromHermes();
}

void CellularESimProfileHandlerImpl::UpdateProfilesFromHermes() {
  // Profiles are stored in prefs.
  if (!device_prefs_)
    return;

  std::vector<CellularESimProfile> profiles_from_hermes =
      GenerateProfilesFromHermes();

  // Ignore empty profile lists provided by Hermes, since this represents either
  // (1) a temporary state during startup, or (2) not actually having any eSIMs.
  if (profiles_from_hermes.empty())
    return;

  std::vector<CellularESimProfile> profiles_before_fetch = GetESimProfiles();

  // If nothing has changed since the last update, do not update prefs or notify
  // observers of a change.
  if (profiles_from_hermes == profiles_before_fetch)
    return;

  // Store the updated list of profiles in prefs.
  base::Value list(base::Value::Type::LIST);
  for (const auto& profile : profiles_from_hermes)
    list.Append(profile.ToDictionaryValue());
  device_prefs_->Set(prefs::kESimProfilesPrefName, std::move(list));

  NotifyESimProfileListUpdated();
}

}  // namespace chromeos
