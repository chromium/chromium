// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_utils.h"

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "crypto/sha2.h"

namespace ash {

namespace cellular_utils {

const char kSmdsGsma[] = "1$lpa.ds.gsma.com$";
const char kSmdsStork[] = "1$prod.smds.rsp.goog$";
const char kSmdsAndroidProduction[] = "1$lpa.live.esimdiscovery.com$";
const char kSmdsAndroidStaging[] = "1$lpa.live.esimdiscovery.dev$";

}  // namespace cellular_utils

namespace {
const char kNonShillCellularNetworkPathPrefix[] = "/non-shill-cellular/";

std::string GetLogSafeEid(const std::string& eid) {
  const SystemSaltGetter::RawSalt* salt = SystemSaltGetter::Get()->GetRawSalt();
  if (!salt) {
    return std::string();
  }
  return crypto::SHA256HashString(
      eid + SystemSaltGetter::ConvertRawSaltToHexString(*salt));
}

}  // namespace

base::flat_set<dbus::ObjectPath> GetProfilePathsFromEuicc(
    HermesEuiccClient::Properties* euicc_properties) {
  base::flat_set<dbus::ObjectPath> profile_paths;

  for (const dbus::ObjectPath& path : euicc_properties->profiles().value()) {
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
      NOTREACHED_IN_MIGRATION() << "Unexpected Hermes profile state: " << state;
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

    // Hermes only exposes eSIM profiles with relevant profile class. e.g.
    // Test profiles are exposed only when Hermes is put into test mode.
    // No additional profile filtering is done on Chrome side.
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

const base::flat_map<int32_t, std::string> GetESimSlotToEidMap() {
  base::flat_map<int32_t, std::string> esim_slot_to_eid;
  const std::vector<dbus::ObjectPath>& available_euiccs =
      HermesManagerClient::Get()->GetAvailableEuiccs();
  VLOG(1) << "GetESimSlotToEidMap(): Num available EUICCs: "
          << available_euiccs.size();
  for (auto& euicc_path : available_euiccs) {
    HermesEuiccClient::Properties* properties =
        HermesEuiccClient::Get()->GetProperties(euicc_path);
    int32_t slot_id = properties->physical_slot().value();
    std::string eid = properties->eid().value();
    esim_slot_to_eid.emplace(slot_id, eid);
    VLOG(1) << "EUICC: " << euicc_path.value() << ", slot id: " << slot_id
            << ", eid: " << GetLogSafeEid(eid);
  }
  return esim_slot_to_eid;
}

namespace cellular_utils {

std::vector<CellularESimProfile> GenerateProfilesFromHermes() {
  std::vector<CellularESimProfile> profiles;

  for (const dbus::ObjectPath& euicc_path :
       HermesManagerClient::Get()->GetAvailableEuiccs()) {
    std::vector<CellularESimProfile> profiles_from_euicc =
        GenerateProfilesFromEuicc(euicc_path);
    base::ranges::copy(profiles_from_euicc, std::back_inserter(profiles));
  }

  return profiles;
}

const DeviceState::CellularSIMSlotInfos GetSimSlotInfosWithUpdatedEid(
    const DeviceState* device) {
  const base::flat_map<int32_t, std::string> esim_slot_to_eid =
      GetESimSlotToEidMap();

  DeviceState::CellularSIMSlotInfos sim_slot_infos = device->GetSimSlotInfos();
  VLOG(1) << "GetSimSlotInfosWithUpdatedEid(): Num SIM slot infos: "
          << sim_slot_infos.size();
  for (auto& sim_slot_info : sim_slot_infos) {
    const std::string shill_provided_eid = sim_slot_info.eid;
    VLOG(1) << "SIM slot id: " << sim_slot_info.slot_id
            << ", Shill provided eid: " << GetLogSafeEid(shill_provided_eid);

    // If there is no associated |slot_id| in the map, the SIM slot info refers
    // to a pSIM, and the Hermes provided data is irrelevant.
    auto it = esim_slot_to_eid.find(sim_slot_info.slot_id);
    if (it == esim_slot_to_eid.end())
      continue;

    const std::string hermes_provided_eid = it->second;
    if (!shill_provided_eid.empty() &&
        hermes_provided_eid != shill_provided_eid) {
      LOG(ERROR) << "Hermes provided EID of " << hermes_provided_eid
                 << " does not match Shill provided non-empty EID of "
                 << shill_provided_eid << ". Defaulting to Shill provided EID.";
    } else {
      sim_slot_info.eid = hermes_provided_eid;
    }
  }
  return sim_slot_infos;
}

bool IsSimPrimary(const std::string& iccid, const DeviceState* device) {
  for (const auto& sim_slot_info : device->GetSimSlotInfos()) {
    if (sim_slot_info.iccid == iccid && sim_slot_info.primary) {
      return true;
    }
  }
  return false;
}

std::string GenerateStubCellularServicePath(const std::string& iccid) {
  return base::StrCat({kNonShillCellularNetworkPathPrefix, iccid});
}

const NetworkProfile* GetCellularProfile(
    const NetworkProfileHandler* network_profile_handler) {
  DCHECK(network_profile_handler);
  return network_profile_handler->GetProfileForUserhash(
      /*userhash=*/std::string());
}

bool IsStubCellularServicePath(const std::string& service_path) {
  return base::StartsWith(service_path, kNonShillCellularNetworkPathPrefix);
}

std::optional<dbus::ObjectPath> GetCurrentEuiccPath() {
  // Always use the first Euicc if Hermes only exposes one Euicc.
  // If useSecondEuicc flag is set and there are two Euicc available,
  // use the second available Euicc.
  const std::vector<dbus::ObjectPath>& euicc_paths =
      HermesManagerClient::Get()->GetAvailableEuiccs();
  if (euicc_paths.empty())
    return std::nullopt;

  if (euicc_paths.size() == 1)
    return euicc_paths[0];

  bool use_second_euicc =
      base::FeatureList::IsEnabled(features::kCellularUseSecondEuicc);
  return use_second_euicc ? euicc_paths[1] : euicc_paths[0];
}

std::vector<std::string> GetSmdsActivationCodes() {
  std::vector<std::string> activation_codes;
  if (features::ShouldUseStorkSmds()) {
    activation_codes.emplace_back(kSmdsStork);
  }
  if (features::ShouldUseAndroidStagingSmds()) {
    activation_codes.emplace_back(kSmdsAndroidStaging);
  }
  if (activation_codes.empty()) {
    activation_codes = {
        kSmdsAndroidProduction,
        kSmdsGsma,
    };
  }
  return activation_codes;
}

}  // namespace cellular_utils
}  // namespace ash
