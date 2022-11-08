// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "components/prefs/pref_service.h"

class PrefService;

namespace ash {

class NetworkStateHandler;

// Source of truth for which eSIM profiles are available on this device.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimProfileHandler
    : public HermesManagerClient::Observer,
      public HermesEuiccClient::Observer,
      public HermesProfileClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when an eSIM profile is added, removed, or updated.
    virtual void OnESimProfileListUpdated() = 0;
  };

  CellularESimProfileHandler(const CellularESimProfileHandler&) = delete;
  CellularESimProfileHandler& operator=(const CellularESimProfileHandler&) =
      delete;
  ~CellularESimProfileHandler() override;

  void Init(NetworkStateHandler* network_state_handler,
            CellularInhibitor* cellular_inhibitor);

  // Callback which returns an InhibitLock. If the InhibitLock returned by the
  // function is null, this means that the operation failed.
  using RefreshProfilesCallback =
      base::OnceCallback<void(std::unique_ptr<CellularInhibitor::InhibitLock>)>;

  // Refreshes the list of installed profiles from Hermes. This operation
  // requires the Cellular Device to be inhibited. If |inhibit_lock| is passed
  // by the client, it will be used; otherwise, this function will acquire one
  // internally. The |RefreshProfileListAndRestoreSlot| variant is identical
  // except that it requests Hermes to maintain SIM slots after refresh.
  //
  // On success, this function returns the lock; on failure, it returns null.
  void RefreshProfileList(
      const dbus::ObjectPath& euicc_path,
      RefreshProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock = nullptr);
  void RefreshProfileListAndRestoreSlot(
      const dbus::ObjectPath& euicc_path,
      RefreshProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock = nullptr);

  // Returns a list of the known cellular eSIM profiles fetched from Hermes.
  // Note that this function returns cached values if an eSIM slot is not active
  // (e.g., if ModemManager is currently pointed to a pSIM slot).
  virtual std::vector<CellularESimProfile> GetESimProfiles() = 0;

  // Returns whether profiles for the EUICC with the given EID or path have been
  // refreshsed. If this function returns true, any known eSIM profiles for the
  // associated EUICC should be returned by GetESimProfiles().
  virtual bool HasRefreshedProfilesForEuicc(const std::string& eid) = 0;
  virtual bool HasRefreshedProfilesForEuicc(
      const dbus::ObjectPath& euicc_path) = 0;

  virtual void SetDevicePrefs(PrefService* device_prefs) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  CellularESimProfileHandler();

  bool has_completed_successful_profile_refresh() const {
    return has_completed_successful_profile_refresh_;
  }

  virtual void OnHermesPropertiesUpdated() = 0;
  void NotifyESimProfileListUpdated();

  const NetworkStateHandler* network_state_handler() const {
    return network_state_handler_;
  }

  NetworkStateHandler* network_state_handler() {
    return network_state_handler_;
  }

  CellularInhibitor* cellular_inhibitor() const { return cellular_inhibitor_; }

  virtual void InitInternal() {}

 private:
  // HermesManagerClient::Observer:
  void OnAvailableEuiccListChanged() override;

  // HermesEuiccClient::Observer:
  void OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                              const std::string& property_name) override;

  // HermesProfileClient::Observer:
  void OnCarrierProfilePropertyChanged(
      const dbus::ObjectPath& carrier_profile_path,
      const std::string& property_name) override;

  void PerformRefreshProfileList(
      const dbus::ObjectPath& euicc_path,
      bool restore_slot,
      RefreshProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock = nullptr);
  void OnInhibited(
      const dbus::ObjectPath& euicc_path,
      bool restore_slot,
      RefreshProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void RefreshProfilesWithLock(
      const dbus::ObjectPath& euicc_path,
      bool restore_slot,
      RefreshProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnRequestInstalledProfilesResult(base::TimeTicks start_time,
                                        HermesResponseStatus status);

  CellularInhibitor* cellular_inhibitor_ = nullptr;

  NetworkStateHandler* network_state_handler_ = nullptr;

  base::ObserverList<Observer> observer_list_;

  // True if the profile list has been refreshed successfully during this
  // session.
  bool has_completed_successful_profile_refresh_ = false;

  // When a profile refresh is in progress, the inhibit lock.
  std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock_;

  // When a profile refresh is in progress, the callback.
  RefreshProfilesCallback callback_;

  base::WeakPtrFactory<CellularESimProfileHandler> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::CellularESimProfileHandler;
}

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_H_
