// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_H_

#include "base/observer_list.h"
#include "chromeos/ash/components/proximity_auth/proximity_monitor_observer.h"

namespace proximity_auth {

// An interface that is responsible for tracking whether the remote device is
// sufficiently close to the local device to permit unlocking.
class ProximityMonitor {
 public:
  ProximityMonitor();
  virtual ~ProximityMonitor();

  void AddObserver(ProximityMonitorObserver* observer);
  void RemoveObserver(ProximityMonitorObserver* observer);

  // Activates the proximity monitor. No-op if the proximity monitor is already
  // active.
  virtual void Start() = 0;

  // Deactivates the proximity monitor. No-op if the proximity monitor is
  // already inactive.
  virtual void Stop() = 0;

  // Returns |true| iff the remote device is close enough to the local device,
  // given the user's current settings.
  virtual bool IsUnlockAllowed() const = 0;

  // Records the current proximity measurements to UMA. This should be called
  // when the user successfully authenticates using proximity auth.
  virtual void RecordProximityMetricsOnAuthSuccess() = 0;

 protected:
  void NotifyProximityStateChanged();

 private:
  // The observers attached to the ProximityMonitor.
  base::ObserverList<ProximityMonitorObserver>::Unchecked observers_;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_H_
