// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker.h"

namespace ash {

namespace tether {

GmsCoreNotificationsStateTracker::GmsCoreNotificationsStateTracker() = default;

GmsCoreNotificationsStateTracker::~GmsCoreNotificationsStateTracker() = default;

void GmsCoreNotificationsStateTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void GmsCoreNotificationsStateTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void GmsCoreNotificationsStateTracker::NotifyGmsCoreNotificationStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnGmsCoreNotificationStateChanged();
}

}  // namespace tether

}  // namespace ash
