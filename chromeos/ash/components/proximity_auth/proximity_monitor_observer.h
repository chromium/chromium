// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_OBSERVER_H_

namespace proximity_auth {

// The observer interface for the ProximityMonitor class.
class ProximityMonitorObserver {
 public:
  virtual ~ProximityMonitorObserver() {}

  // Callback to be called when the proximity state of the remote device
  // changes.
  virtual void OnProximityStateChanged() = 0;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_OBSERVER_H_
