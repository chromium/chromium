// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_

#include "base/containers/flat_set.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_manager.h"

namespace chromeos {
namespace phonehub {

// TODO(https://crbug.com/1106937): Add real implementation.
class NotificationManagerImpl : public NotificationManager {
 public:
  NotificationManagerImpl();
  ~NotificationManagerImpl() override;

 private:
  // NotificationManager:
  const Notification* GetNotification(int64_t notification_id) const override;
  void SetNotificationsInternal(
      const base::flat_set<Notification>& notifications) override;
  void RemoveNotificationsInternal(
      const base::flat_set<int64_t>& notification_ids) override;
  void ClearNotificationsInternal() override;
  void DismissNotification(int64_t notification_id) override;
  void SendInlineReply(int64_t notification_id,
                       const base::string16& inline_reply_text) override;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
