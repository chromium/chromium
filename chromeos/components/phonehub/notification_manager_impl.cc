// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_manager_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/notification.h"

namespace chromeos {
namespace phonehub {

NotificationManagerImpl::NotificationManagerImpl() = default;

NotificationManagerImpl::~NotificationManagerImpl() = default;

const Notification* NotificationManagerImpl::GetNotification(
    int64_t notification_id) const {
  return nullptr;
}

void NotificationManagerImpl::SetNotificationsInternal(
    const base::flat_set<Notification>& notifications) {
  PA_LOG(INFO) << "Setting notifications internally.";
  // TODO(jimmyxong): Implement this stub function.
}

void NotificationManagerImpl::RemoveNotificationsInternal(
    const base::flat_set<int64_t>& notification_ids) {
  PA_LOG(INFO) << "Removing notifications internally.";
  // TODO(jimmyxgong): Implement this stub function.
}

void NotificationManagerImpl::DismissNotification(int64_t notification_id) {
  PA_LOG(INFO) << "Dismissing notification with ID " << notification_id << ".";
}

void NotificationManagerImpl::ClearNotificationsInternal() {
  PA_LOG(INFO) << "Clearing notification internally.";
  // TODO(jimmyxgong): Implement this stub function.
}

void NotificationManagerImpl::SendInlineReply(
    int64_t notification_id,
    const base::string16& inline_reply_text) {
  PA_LOG(INFO) << "Sending inline reply for notification with ID "
               << notification_id << ".";
}

}  // namespace phonehub
}  // namespace chromeos
