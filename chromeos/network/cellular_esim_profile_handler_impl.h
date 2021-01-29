// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_

#include "base/component_export.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile_handler.h"

class PrefService;
class PrefRegistrySimple;

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimProfileHandlerImpl
    : public CellularESimProfileHandler,
      public HermesManagerClient::Observer,
      public HermesEuiccClient::Observer,
      public HermesProfileClient::Observer {
 public:
  CellularESimProfileHandlerImpl();
  CellularESimProfileHandlerImpl(const CellularESimProfileHandlerImpl&) =
      delete;
  CellularESimProfileHandlerImpl& operator=(
      const CellularESimProfileHandlerImpl&) = delete;
  ~CellularESimProfileHandlerImpl() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  // CellularESimProfileHandler:
  void Init() override;
  std::vector<CellularESimProfile> GetESimProfiles() override;
  void SetDevicePrefs(PrefService* device_prefs) override;

  // HermesManagerClient::Observer:
  void OnAvailableEuiccListChanged() override;

  // HermesEuiccClient::Observer:
  void OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                              const std::string& property_name) override;

  // HermesProfileClient::Observer:
  void OnCarrierProfilePropertyChanged(
      const dbus::ObjectPath& carrier_profile_path,
      const std::string& property_name) override;

  void UpdateProfilesFromHermes();

  // Initialized to null and set once SetDevicePrefs() is called.
  PrefService* device_prefs_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
