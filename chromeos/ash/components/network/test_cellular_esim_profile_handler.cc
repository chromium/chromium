// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

TestCellularESimProfileHandler::TestCellularESimProfileHandler()
    : enable_notify_profile_list_update_(true),
      has_pending_notify_list_update_(false) {}

TestCellularESimProfileHandler::~TestCellularESimProfileHandler() = default;

void TestCellularESimProfileHandler::SetHasRefreshedProfilesForEuicc(
    const std::string& eid,
    const dbus::ObjectPath& euicc_path,
    bool has_refreshed) {
  if (has_refreshed) {
    refreshed_eids_.insert(eid);
    refreshed_euicc_paths_.insert(euicc_path);
    return;
  }

  refreshed_eids_.erase(eid);
  refreshed_euicc_paths_.erase(euicc_path);
}

void TestCellularESimProfileHandler::SetEnableNotifyProfileListUpdate(
    bool enable_notify_profile_list_update) {
  enable_notify_profile_list_update_ = enable_notify_profile_list_update;
  if (enable_notify_profile_list_update_ && has_pending_notify_list_update_) {
    NotifyESimProfileListUpdated();
    has_pending_notify_list_update_ = false;
  }
}

std::vector<CellularESimProfile>
TestCellularESimProfileHandler::GetESimProfiles() {
  return esim_profile_states_;
}

bool TestCellularESimProfileHandler::HasRefreshedProfilesForEuicc(
    const std::string& eid) {
  return base::Contains(refreshed_eids_, eid);
}

bool TestCellularESimProfileHandler::HasRefreshedProfilesForEuicc(
    const dbus::ObjectPath& euicc_path) {
  return base::Contains(refreshed_euicc_paths_, euicc_path);
}

void TestCellularESimProfileHandler::SetDevicePrefs(PrefService* device_prefs) {
}

void TestCellularESimProfileHandler::OnHermesPropertiesUpdated() {
  std::vector<CellularESimProfile> new_profile_states =
      cellular_utils::GenerateProfilesFromHermes();
  if (new_profile_states == esim_profile_states_)
    return;
  esim_profile_states_ = new_profile_states;

  network_state_handler()->SyncStubCellularNetworks();
  if (!enable_notify_profile_list_update_) {
    has_pending_notify_list_update_ = true;
    return;
  }
  NotifyESimProfileListUpdated();
}

}  // namespace ash
