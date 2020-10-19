// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_manager.h"

#include "chromeos/components/multidevice/logging/logging.h"

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

void NotificationManager::SetNotificationsInternal(
    const base::flat_set<Notification>& notifications) {
  PA_LOG(INFO) << "Setting notifications internally.";

  base::flat_set<int64_t> added_ids;
  base::flat_set<int64_t> updated_ids;

  for (const Notification& notification : notifications) {
    int64_t id = notification.id();
    auto it = id_to_notification_map_.find(id);

    if (it == id_to_notification_map_.end()) {
      id_to_notification_map_.emplace(id, notification);
      added_ids.emplace(id);
      continue;
    }

    it->second = notification;
    updated_ids.emplace(id);
  }

  NotifyNotificationsAdded(added_ids);
  NotifyNotificationsUpdated(updated_ids);
}

void NotificationManager::RemoveNotificationsInternal(
    const base::flat_set<int64_t>& notification_ids) {
  PA_LOG(INFO) << "Removing notifications internally.";

  for (int64_t id : notification_ids) {
    auto it = id_to_notification_map_.find(id);
    if (it == id_to_notification_map_.end())
      continue;

    id_to_notification_map_.erase(it);
  }

  NotifyNotificationsRemoved(notification_ids);
}

void NotificationManager::ClearNotificationsInternal() {
  PA_LOG(INFO) << "Clearing notification internally.";

  base::flat_set<int64_t> removed_ids;
  for (const auto& pair : id_to_notification_map_) {
    removed_ids.emplace(pair.first);
  }

  id_to_notification_map_.clear();
  NotifyNotificationsRemoved(removed_ids);
}

const Notification* NotificationManager::GetNotification(
    int64_t notification_id) const {
  auto it = id_to_notification_map_.find(notification_id);
  if (it == id_to_notification_map_.end())
    return nullptr;
  return &it->second;
}

}  // namespace phonehub
}  // namespace chromeos
