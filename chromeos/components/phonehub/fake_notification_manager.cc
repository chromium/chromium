// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_notification_manager.h"

#include "base/check.h"

namespace chromeos {
namespace phonehub {

FakeNotificationManager::InlineReplyMetadata::InlineReplyMetadata(
    int64_t notification_id,
    const base::string16& inline_reply_text)
    : notification_id(notification_id), inline_reply_text(inline_reply_text) {}

FakeNotificationManager::InlineReplyMetadata::~InlineReplyMetadata() = default;

FakeNotificationManager::FakeNotificationManager() = default;

FakeNotificationManager::~FakeNotificationManager() = default;

void FakeNotificationManager::SetNotification(
    const Notification& notification) {
  SetNotificationsInternal(base::flat_set<Notification>{notification});
}

void FakeNotificationManager::SetNotificationsInternal(
    const base::flat_set<Notification>& notifications) {
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

void FakeNotificationManager::RemoveNotification(int64_t id) {
  RemoveNotificationsInternal(base::flat_set<int64_t>{id});
}

void FakeNotificationManager::RemoveNotificationsInternal(
    const base::flat_set<int64_t>& ids) {
  for (int64_t id : ids) {
    auto it = id_to_notification_map_.find(id);
    DCHECK(it != id_to_notification_map_.end());
    id_to_notification_map_.erase(it);
  }

  NotifyNotificationsRemoved(ids);
}

void FakeNotificationManager::ClearNotificationsInternal() {
  base::flat_set<int64_t> removed_ids;
  for (const auto& pair : id_to_notification_map_) {
    removed_ids.emplace(pair.first);
  }

  id_to_notification_map_.clear();
  NotifyNotificationsRemoved(removed_ids);
}

const Notification* FakeNotificationManager::GetNotification(
    int64_t notification_id) const {
  auto it = id_to_notification_map_.find(notification_id);
  if (it == id_to_notification_map_.end())
    return nullptr;
  return &it->second;
}

void FakeNotificationManager::DismissNotification(int64_t notification_id) {
  DCHECK(base::Contains(id_to_notification_map_, notification_id));
  dismissed_notification_ids_.push_back(notification_id);
  NotifyNotificationsRemoved(base::flat_set<int64_t>{notification_id});
}

void FakeNotificationManager::SendInlineReply(
    int64_t notification_id,
    const base::string16& inline_reply_text) {
  DCHECK(base::Contains(id_to_notification_map_, notification_id));
  inline_replies_.emplace_back(notification_id, inline_reply_text);
}

}  // namespace phonehub
}  // namespace chromeos
