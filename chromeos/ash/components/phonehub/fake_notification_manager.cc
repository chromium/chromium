// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_notification_manager.h"

#include "base/check.h"
#include "base/containers/contains.h"

namespace ash {
namespace phonehub {

FakeNotificationManager::InlineReplyMetadata::InlineReplyMetadata(
    int64_t notification_id,
    const std::u16string& inline_reply_text)
    : notification_id(notification_id), inline_reply_text(inline_reply_text) {}

FakeNotificationManager::InlineReplyMetadata::~InlineReplyMetadata() = default;

FakeNotificationManager::FakeNotificationManager() = default;

FakeNotificationManager::~FakeNotificationManager() = default;

void FakeNotificationManager::SetNotification(
    const Notification& notification) {
  SetNotificationsInternal(base::flat_set<Notification>{notification});
}

void FakeNotificationManager::RemoveNotification(int64_t id) {
  RemoveNotificationsInternal(base::flat_set<int64_t>{id});
}

void FakeNotificationManager::DismissNotification(int64_t notification_id) {
  DCHECK(base::Contains(id_to_notification_map_, notification_id));
  dismissed_notification_ids_.push_back(notification_id);
  NotifyNotificationsRemoved(base::flat_set<int64_t>{notification_id});
}

void FakeNotificationManager::SendInlineReply(
    int64_t notification_id,
    const std::u16string& inline_reply_text) {
  DCHECK(base::Contains(id_to_notification_map_, notification_id));
  inline_replies_.emplace_back(notification_id, inline_reply_text);
}

}  // namespace phonehub
}  // namespace ash
