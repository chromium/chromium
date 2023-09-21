// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_WAITER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_WAITER_H_

#include <string>

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace ash {

// This class performs a best-effort attempt at waiting until specified Hermes
// profiles meet specified conditions regarding their properties to invoke a
// provided callback. The provided callback will be invoked if the destructor is
// called and the callback hadn't previously been invoked. This guarantee allows
// clients to pass ownership of instances of this class to delayed tasks,
// resulting in the callback being called as soon as the conditions are met or
// upon destruction when the task completes.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimProfileWaiter
    : public HermesManagerClient::Observer,
      public HermesProfileClient::Observer {
 public:
  // Comparison function that should return |true| IFF the provided Hermes
  // properties have reached a desired state.
  using Condition =
      base::RepeatingCallback<bool(HermesProfileClient::Properties*)>;

  CellularESimProfileWaiter();
  CellularESimProfileWaiter(const CellularESimProfileWaiter&) = delete;
  CellularESimProfileWaiter& operator=(const CellularESimProfileWaiter&) =
      delete;
  ~CellularESimProfileWaiter() override;

  // This is a helper function used to add a condition that the profile at
  // |profile_path| has the properties that are expected of a pending eSIM
  // profile to be set.
  void RequirePendingProfile(const dbus::ObjectPath& profile_path);

  // This function is used to begin watching for Hermes profile property
  // changes. This function will perform an initial check of whether all of the
  // conditions have already been met; if so, the |on_success| callback will be
  // immediately called. Clients should confirm whether they need to continue
  // waiting after calling this method since all conditions may be satisfied
  // without delay.
  //
  // If the Hermes clients are shut down and the specified conditions have not
  // been met the |on_shutdown| callback will be called.
  void Wait(base::OnceCallback<void()> on_success,
            base::OnceCallback<void()> on_shutdown);

  // Returns whether we are waiting for all conditions to be met.
  bool waiting() const;

 private:
  friend class CellularESimProfileWaiterTest;

  // HermesManagerClient::Observer:
  void OnShutdown() override;

  // HermesProfileClient::Observer:
  void OnCarrierProfilePropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) override;

  // This function is used to add a condition to wait on. The |profile_path|
  // argument should be the path to an Hermes profile and |condition| should be
  // a function that evaluates |true| IFF the profile is in the desired state.
  // The profile does not need to exist at the time this method is called.
  void AddCondition(const dbus::ObjectPath& profile_path, Condition condition);

  // This function returns whether |condition| is satisfied for the Hermes
  // profile at |profile_path|.
  bool EvaluateCondition(const dbus::ObjectPath& profile_path,
                         const Condition& condition);

  // This function checks whether all conditions have been met and is
  // responsible for stopping observing Hermes profile property changes and
  // invoking |on_success_|.
  void MaybeFinish();

  base::flat_map<dbus::ObjectPath, Condition> profile_path_to_condition_;

  // This will be called when all specified conditions have been met, or upon
  // destruction.
  base::OnceCallback<void()> on_success_;

  // This will be called IFF all specified conditions have not been met and the
  // Hermes clients are being shut down.
  base::OnceCallback<void()> on_shutdown_;

  base::ScopedObservation<HermesManagerClient, HermesManagerClient::Observer>
      hermes_manager_client_observer_{this};
  base::ScopedObservation<HermesProfileClient, HermesProfileClient::Observer>
      hermes_profile_client_observer_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_WAITER_H_
