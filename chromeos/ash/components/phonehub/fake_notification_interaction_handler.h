// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_INTERACTION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_INTERACTION_HANDLER_H_

#include <stdint.h>

#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_interaction_handler.h"

namespace ash {
namespace phonehub {

class FakeNotificationInteractionHandler
    : public NotificationInteractionHandler {
 public:
  FakeNotificationInteractionHandler();
  ~FakeNotificationInteractionHandler() override;

  size_t handled_notification_count() const {
    return handled_notification_count_;
  }

  size_t notification_click_handler_count() const {
    return notification_click_handler_count_;
  }

  void AddNotificationClickHandler(NotificationClickHandler* handler) override;
  void RemoveNotificationClickHandler(
      NotificationClickHandler* handler) override;

 private:
  void HandleNotificationClicked(
      int64_t notification_id,
      const Notification::AppMetadata& app_metadata) override;
  size_t handled_notification_count_ = 0;
  size_t notification_click_handler_count_ = 0;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_NOTIFICATION_INTERACTION_HANDLER_H_
