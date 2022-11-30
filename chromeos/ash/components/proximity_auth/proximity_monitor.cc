// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/proximity_monitor.h"

namespace proximity_auth {

ProximityMonitor::ProximityMonitor() = default;

ProximityMonitor::~ProximityMonitor() = default;

void ProximityMonitor::AddObserver(ProximityMonitorObserver* observer) {
  observers_.AddObserver(observer);
}

void ProximityMonitor::RemoveObserver(ProximityMonitorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ProximityMonitor::NotifyProximityStateChanged() {
  for (auto& observer : observers_)
    observer.OnProximityStateChanged();
}

}  // namespace proximity_auth
