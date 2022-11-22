// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_CELLULAR_PREF_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_CELLULAR_PREF_HANDLER_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_service.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

class NetworkStateHandler;

// This class provides the ability to store and query prefs for managed cellular
// networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ManagedCellularPrefHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when a ICCID - SMDP address is added or removed.
    virtual void OnManagedCellularPrefChanged() = 0;
  };

  ManagedCellularPrefHandler();
  ManagedCellularPrefHandler(const ManagedCellularPrefHandler&) = delete;
  ManagedCellularPrefHandler& operator=(const ManagedCellularPrefHandler&) =
      delete;
  virtual ~ManagedCellularPrefHandler();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void Init(NetworkStateHandler* network_state_handler);
  void SetDevicePrefs(PrefService* device_prefs);

  // Add a new ICCID and SMDP+ address pair to device pref for a managed
  // cellular network.
  void AddIccidSmdpPair(const std::string& iccid,
                        const std::string& smdp_address);

  // Remove the ICCID and SMDP+ address pair from the device pref with given
  // |iccid|.
  void RemovePairWithIccid(const std::string& iccid);

  // Marks cellular network with iccid |iccid| as migrated to the APN revamp
  // feature. See (b/162365553).
  virtual void AddApnMigratedIccid(const std::string& iccid);

  // Return true if the |iccid| has been migrated to the APN Revamp feature.
  virtual bool ContainsApnMigratedIccid(const std::string& iccid) const;

  // Returns the corresponding SMDP+ address for the given |iccid|. Returns
  // nullptr if no such |iccid| is found.
  const std::string* GetSmdpAddressFromIccid(const std::string& iccid) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  void NotifyManagedCellularPrefChanged();

  NetworkStateHandler* network_state_handler_ = nullptr;
  // Initialized to null and set once SetDevicePrefs() is called.
  PrefService* device_prefs_ = nullptr;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_CELLULAR_PREF_HANDLER_H_
