// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_H_

#include <vector>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "components/prefs/pref_service.h"

class PrefService;

namespace chromeos {

// Source of truth for which eSIM profiles are available on this device.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimProfileHandler {
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
  virtual ~CellularESimProfileHandler();

  virtual void Init() = 0;

  // Returns a list of the known cellular eSIM profiles fetched from Hermes.
  // Note that this function returns cached values if an eSIM slot is not active
  // (e.g., if ModemManager is currently pointed to a pSIM slot).
  virtual std::vector<CellularESimProfile> GetESimProfiles() = 0;

  virtual void SetDevicePrefs(PrefService* device_prefs) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  CellularESimProfileHandler();

  void NotifyESimProfileListUpdated();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_H_
