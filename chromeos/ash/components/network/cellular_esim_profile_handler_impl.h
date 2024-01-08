// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {
namespace network_ui {
class NetworkConfigMessageHandler;
}

// CellularESimProfileHandler implementation which utilizes the local state
// PrefService to track eSIM profiles.
//
// eSIM profiles can only be retrieved from the device hardware when an EUICC is
// the "active" slot on the device, and only one slot can be active at a time.
// This means that if the physical SIM slot is active, we cannot fetch an
// updated list of profiles without switching slots, which can be disruptive if
// the user is utilizing a cellular connection from the physical SIM slot. To
// ensure that clients can access eSIM metadata regardless of the active slot,
// this class stores all known eSIM profiles persistently in prefs.
//
// Additionally, this class tracks all known EUICC paths. If it detects a new
// EUICC which it previously had not known about, it automatically refreshes
// profile metadata from that slot. This ensures that after a powerwash, since
// all local data will be erased and we will no longer have information on which
// slots we have metadata for, we will refresh the metadata for all slots.
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
  friend class network_ui::NetworkConfigMessageHandler;

  // CellularESimProfileHandler:
  void InitInternal() override;
  std::vector<CellularESimProfile> GetESimProfiles() override;
  bool HasRefreshedProfilesForEuicc(const std::string& eid) override;
  bool HasRefreshedProfilesForEuicc(
      const dbus::ObjectPath& euicc_path) override;
  void SetDevicePrefs(PrefService* device_prefs) override;
  void OnHermesPropertiesUpdated() override;

  void AutoRefreshEuiccsIfNecessary();
  void StartAutoRefresh(const base::flat_set<std::string>& euicc_paths);
  base::flat_set<std::string> GetAutoRefreshedEuiccPaths() const;
  base::flat_set<std::string> GetAutoRefreshedEuiccPathsFromPrefs() const;
  void OnAutoRefreshEuiccComplete(
      const std::string& path,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void AddNewlyRefreshedEuiccPathToPrefs(const std::string& path);

  void UpdateProfilesFromHermes();

  bool CellularDeviceExists() const;

  // Used by chrome://network debug page; not meant to be called during normal
  // usage.
  void ResetESimProfileCache();
  // Disabled the current active eSIM profile.
  void DisableActiveESimProfile();

  void PerformDisableProfile(
      const dbus::ObjectPath& profile_path,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnProfileDisabled(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status);

  // Initialized to null and set once SetDevicePrefs() is called.
  raw_ptr<PrefService> device_prefs_ = nullptr;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::flat_set<std::string> paths_pending_auto_refresh_;

  base::WeakPtrFactory<CellularESimProfileHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
