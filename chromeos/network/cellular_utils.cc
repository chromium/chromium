// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_utils.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile.h"

namespace chromeos {

namespace {

base::flat_set<dbus::ObjectPath> GetProfilePathsFromEuicc(
    HermesEuiccClient::Properties* euicc_properties) {
  base::flat_set<dbus::ObjectPath> profile_paths;

  for (const dbus::ObjectPath& path :
       euicc_properties->installed_carrier_profiles().value()) {
    profile_paths.insert(path);
  }
  for (const dbus::ObjectPath& path :
       euicc_properties->pending_carrier_profiles().value()) {
    profile_paths.insert(path);
  }

  return profile_paths;
}

CellularESimProfile::State FromProfileState(hermes::profile::State state) {
  switch (state) {
    case hermes::profile::State::kPending:
      return CellularESimProfile::State::kPending;
    case hermes::profile::State::kInactive:
      return CellularESimProfile::State::kInactive;
    case hermes::profile::State::kActive:
      return CellularESimProfile::State::kActive;
    default:
      NOTREACHED() << "Unexpected Hermes profile state: " << state;
      return CellularESimProfile::State::kInactive;
  }
}

std::vector<CellularESimProfile> GenerateProfilesFromEuicc(
    const dbus::ObjectPath& euicc_path) {
  std::vector<CellularESimProfile> profiles;

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  std::string eid = euicc_properties->eid().value();

  for (const dbus::ObjectPath& profile_path :
       GetProfilePathsFromEuicc(euicc_properties)) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    profiles.emplace_back(
        FromProfileState(profile_properties->state().value()), profile_path,
        eid, profile_properties->iccid().value(),
        base::UTF8ToUTF16(profile_properties->name().value()),
        base::UTF8ToUTF16(profile_properties->nick_name().value()),
        base::UTF8ToUTF16(profile_properties->service_provider().value()),
        profile_properties->activation_code().value());
  }

  return profiles;
}

}  // namespace

std::vector<CellularESimProfile> GenerateProfilesFromHermes() {
  std::vector<CellularESimProfile> profiles;

  for (const dbus::ObjectPath& euicc_path :
       HermesManagerClient::Get()->GetAvailableEuiccs()) {
    std::vector<CellularESimProfile> profiles_from_euicc =
        GenerateProfilesFromEuicc(euicc_path);
    std::copy(profiles_from_euicc.begin(), profiles_from_euicc.end(),
              std::back_inserter(profiles));
  }

  return profiles;
}

}  // namespace chromeos