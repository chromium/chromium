// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_
#define CHROMEOS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_

#include "chromeos/network/cellular_esim_profile_handler.h"

namespace chromeos {

// A Test implementation of CellularESimProfileHandler that stores profile list
// in-memory and fetches esim profiles directly from the fake hermes clients.
class TestCellularESimProfileHandler : public CellularESimProfileHandler {
 public:
  TestCellularESimProfileHandler();
  ~TestCellularESimProfileHandler() override;

  // CellularESimProfileHandler:
  std::vector<CellularESimProfile> GetESimProfiles() override;
  void SetDevicePrefs(PrefService* device_prefs) override;
  void OnHermesPropertiesUpdated() override;

 private:
  std::vector<CellularESimProfile> esim_profile_states_;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_