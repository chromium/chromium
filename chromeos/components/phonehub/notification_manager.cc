// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_manager.h"

namespace chromeos {
namespace phonehub {

NotificationManager::NotificationManager() = default;

NotificationManager::~NotificationManager() = default;

void NotificationManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void NotificationManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void NotificationManager::NotifyNotificationsAdded(
    const base::flat_set<int64_t>& notification_ids) {
  for (auto& observer : observer_list_)
    observer.OnNotificationsAdded(notification_ids);
}

void NotificationManager::NotifyNotificationsUpdated(
    const base::flat_set<int64_t>& notification_ids) {
  for (auto& observer : observer_list_)
    observer.OnNotificationsUpdated(notification_ids);
}

void NotificationManager::NotifyNotificationsRemoved(
    const base::flat_set<int64_t>& notification_ids) {
  for (auto& observer : observer_list_)
    observer.OnNotificationsRemoved(notification_ids);
}

}  // namespace phonehub
}  // namespace chromeos
