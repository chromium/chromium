// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_

#include <unordered_map>

#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_manager.h"

namespace chromeos {
namespace phonehub {

class MessageSender;

class NotificationManagerImpl : public NotificationManager {
 public:
  NotificationManagerImpl(MessageSender* message_sender);
  ~NotificationManagerImpl() override;

 private:
  // NotificationManager:
  void DismissNotification(int64_t notification_id) override;
  void SendInlineReply(int64_t notification_id,
                       const base::string16& inline_reply_text) override;

  MessageSender* message_sender_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
