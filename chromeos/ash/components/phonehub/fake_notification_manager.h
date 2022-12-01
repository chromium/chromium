// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_MANAGER_H_

#include <vector>

#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_manager.h"

namespace ash {
namespace phonehub {

class FakeNotificationManager : public NotificationManager {
 public:
  FakeNotificationManager();
  ~FakeNotificationManager() override;

  using NotificationManager::SetNotificationsInternal;

  using NotificationManager::RemoveNotificationsInternal;

  using NotificationManager::ClearNotificationsInternal;

  void SetNotification(const Notification& notification);

  void RemoveNotification(int64_t id);

  const std::vector<int64_t>& dismissed_notification_ids() const {
    return dismissed_notification_ids_;
  }

  size_t num_notifications() const { return id_to_notification_map_.size(); }

  struct InlineReplyMetadata {
    InlineReplyMetadata(int64_t notification_id,
                        const std::u16string& inline_reply_text);
    ~InlineReplyMetadata();

    int64_t notification_id;
    std::u16string inline_reply_text;
  };

  const std::vector<InlineReplyMetadata>& inline_replies() const {
    return inline_replies_;
  }

 private:
  // NotificationManager:
  void DismissNotification(int64_t notification_id) override;
  void SendInlineReply(int64_t notification_id,
                       const std::u16string& inline_reply_text) override;

  std::vector<int64_t> dismissed_notification_ids_;
  std::vector<InlineReplyMetadata> inline_replies_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_MANAGER_H_
