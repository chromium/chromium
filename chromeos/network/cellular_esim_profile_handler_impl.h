// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

class PrefService;
class PrefRegistrySimple;

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimProfileHandlerImpl
    : public CellularESimProfileHandler,
      public NetworkStateHandlerObserver {
 public:
  CellularESimProfileHandlerImpl();
  CellularESimProfileHandlerImpl(const CellularESimProfileHandlerImpl&) =
      delete;
  CellularESimProfileHandlerImpl& operator=(
      const CellularESimProfileHandlerImpl&) = delete;
  ~CellularESimProfileHandlerImpl() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;

 private:
  friend class CellularESimProfileHandlerImplTest;

  // CellularESimProfileHandler:
  void InitInternal() override;
  std::vector<CellularESimProfile> GetESimProfiles() override;
  void SetDevicePrefs(PrefService* device_prefs) override;
  void OnHermesPropertiesUpdated() override;

  void RefreshEuiccsIfNecessary();
  base::flat_set<std::string> GetEuiccPathsFromPrefs() const;
  void StoreEuiccPathsToPrefs(const base::flat_set<std::string>& paths);
  void UpdateProfilesFromHermes();
  bool CellularDeviceExists() const;

  // Initialized to null and set once SetDevicePrefs() is called.
  PrefService* device_prefs_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
