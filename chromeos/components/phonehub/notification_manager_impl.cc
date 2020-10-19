// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_manager_impl.h"

#include "base/containers/flat_set.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/message_sender.h"
#include "chromeos/components/phonehub/notification.h"

namespace chromeos {
namespace phonehub {

NotificationManagerImpl::NotificationManagerImpl(MessageSender* message_sender)
    : message_sender_(message_sender) {
  DCHECK(message_sender_);
}

NotificationManagerImpl::~NotificationManagerImpl() = default;

void NotificationManagerImpl::DismissNotification(int64_t notification_id) {
  PA_LOG(INFO) << "Dismissing notification with ID " << notification_id << ".";

  if (!GetNotification(notification_id)) {
    PA_LOG(WARNING) << "Attempted to dismiss an invalid notification with id: "
                    << notification_id << ".";
    return;
  }

  RemoveNotificationsInternal(base::flat_set<int64_t>{notification_id});
  message_sender_->SendDismissNotificationRequest(notification_id);
}

void NotificationManagerImpl::SendInlineReply(
    int64_t notification_id,
    const base::string16& inline_reply_text) {
  if (!GetNotification(notification_id)) {
    PA_LOG(INFO) << "Could not send inline reply for notification with ID "
                 << notification_id << ".";
    return;
  }

  PA_LOG(INFO) << "Sending inline reply for notification with ID "
               << notification_id << ".";
  message_sender_->SendNotificationInlineReplyRequest(notification_id,
                                                      inline_reply_text);
}

}  // namespace phonehub
}  // namespace chromeos
