// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_MANAGER_H_

#include <unordered_map>
#include <vector>

#include "base/containers/flat_set.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_manager.h"

namespace chromeos {
namespace phonehub {

class FakeNotificationManager : public NotificationManager {
 public:
  FakeNotificationManager();
  ~FakeNotificationManager() override;

  void SetNotification(const Notification& notification);
  void SetNotificationsInternal(
      const base::flat_set<Notification>& notifications) override;

  void RemoveNotification(int64_t id);
  void RemoveNotificationsInternal(const base::flat_set<int64_t>& ids) override;

  void ClearNotificationsInternal() override;

  const std::vector<int64_t>& dismissed_notification_ids() const {
    return dismissed_notification_ids_;
  }

  size_t num_notifications() const { return id_to_notification_map_.size(); }

  struct InlineReplyMetadata {
    InlineReplyMetadata(int64_t notification_id,
                        const base::string16& inline_reply_text);
    ~InlineReplyMetadata();

    int64_t notification_id;
    base::string16 inline_reply_text;
  };

  const std::vector<InlineReplyMetadata>& inline_replies() const {
    return inline_replies_;
  }

 private:
  // NotificationManager:
  const Notification* GetNotification(int64_t notification_id) const override;
  void DismissNotification(int64_t notification_id) override;
  void SendInlineReply(int64_t notification_id,
                       const base::string16& inline_reply_text) override;

  std::unordered_map<int64_t, Notification> id_to_notification_map_;
  std::vector<int64_t> dismissed_notification_ids_;
  std::vector<InlineReplyMetadata> inline_replies_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_MANAGER_H_
