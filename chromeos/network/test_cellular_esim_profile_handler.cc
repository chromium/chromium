// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/test_cellular_esim_profile_handler.h"

#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/cellular_utils.h"

namespace chromeos {

TestCellularESimProfileHandler::TestCellularESimProfileHandler() = default;

TestCellularESimProfileHandler::~TestCellularESimProfileHandler() {
  HermesManagerClient::Get()->RemoveObserver(this);
  HermesEuiccClient::Get()->RemoveObserver(this);
  HermesProfileClient::Get()->RemoveObserver(this);
}

void TestCellularESimProfileHandler::Init() {
  HermesManagerClient::Get()->AddObserver(this);
  HermesEuiccClient::Get()->AddObserver(this);
  HermesProfileClient::Get()->AddObserver(this);
}

void TestCellularESimProfileHandler::OnAvailableEuiccListChanged() {
  UpdateESimProfiles();
}

void TestCellularESimProfileHandler::OnEuiccPropertyChanged(
    const dbus::ObjectPath& euicc_path,
    const std::string& property_name) {
  UpdateESimProfiles();
}

void TestCellularESimProfileHandler::OnCarrierProfilePropertyChanged(
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& property_name) {
  UpdateESimProfiles();
}

std::vector<CellularESimProfile>
TestCellularESimProfileHandler::GetESimProfiles() {
  return esim_profile_states_;
}

void TestCellularESimProfileHandler::SetDevicePrefs(PrefService* device_prefs) {
}

void TestCellularESimProfileHandler::UpdateESimProfiles() {
  std::vector<CellularESimProfile> new_profile_states_ =
      GenerateProfilesFromHermes();
  if (new_profile_states_ == esim_profile_states_) {
    return;
  }
  this->esim_profile_states_ = new_profile_states_;
  NotifyESimProfileListUpdated();
  return;
}

}  // namespace chromeos