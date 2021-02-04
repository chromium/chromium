// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_
#define CHROMEOS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_

#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile_handler.h"

namespace chromeos {

// A Test implementation of CellularESimProfileHandler that stores profile list
// in-memory and fetches esim profiles directly from the fake hermes clients.
class TestCellularESimProfileHandler : public CellularESimProfileHandler,
                                       public HermesManagerClient::Observer,
                                       public HermesEuiccClient::Observer,
                                       public HermesProfileClient::Observer {
 public:
  TestCellularESimProfileHandler();
  ~TestCellularESimProfileHandler() override;

  void Init() override;

  // HermesManagerClient::Observer:
  void OnAvailableEuiccListChanged() override;

  // HermesEuiccClient::Observer:
  void OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                              const std::string& property_name) override;

  // HermesProfileClient::Observer:
  void OnCarrierProfilePropertyChanged(
      const dbus::ObjectPath& carrier_profile_path,
      const std::string& property_name) override;

  // CellularESimProfileHandler:
  std::vector<CellularESimProfile> GetESimProfiles() override;
  void SetDevicePrefs(PrefService* device_prefs) override;

 private:
  void UpdateESimProfiles();

  std::vector<CellularESimProfile> esim_profile_states_;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_