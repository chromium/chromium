// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_INTERACTION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_INTERACTION_HANDLER_H_

#include <stdint.h>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_click_handler.h"

namespace ash {
namespace phonehub {

// The handler that exposes the APIs to interact with Phone Hub Notification.
class NotificationInteractionHandler {
 public:
  virtual ~NotificationInteractionHandler();

  // Called by PhoneHubNotificationController to notify the click event.
  virtual void HandleNotificationClicked(
      int64_t notification_id,
      const Notification::AppMetadata& app_metadata) = 0;

  virtual void AddNotificationClickHandler(NotificationClickHandler* handler);
  virtual void RemoveNotificationClickHandler(
      NotificationClickHandler* handler);

 protected:
  NotificationInteractionHandler();
  void NotifyNotificationClicked(int64_t notification_id,
                                 const Notification::AppMetadata& app_metadata);

 private:
  base::ObserverList<NotificationClickHandler> handler_list_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_INTERACTION_HANDLER_H_
