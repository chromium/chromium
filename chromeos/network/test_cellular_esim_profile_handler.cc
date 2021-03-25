// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/test_cellular_esim_profile_handler.h"

#include "chromeos/network/cellular_utils.h"

namespace chromeos {

TestCellularESimProfileHandler::TestCellularESimProfileHandler() = default;

TestCellularESimProfileHandler::~TestCellularESimProfileHandler() = default;

std::vector<CellularESimProfile>
TestCellularESimProfileHandler::GetESimProfiles() {
  return esim_profile_states_;
}

void TestCellularESimProfileHandler::SetDevicePrefs(PrefService* device_prefs) {
}

void TestCellularESimProfileHandler::OnHermesPropertiesUpdated() {
  std::vector<CellularESimProfile> new_profile_states =
      GenerateProfilesFromHermes();
  if (new_profile_states == esim_profile_states_)
    return;

  esim_profile_states_ = new_profile_states;
  NotifyESimProfileListUpdated();
}

}  // namespace chromeos