// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_

#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/notification.h"

namespace ash {
namespace phonehub {

// Handles actions performed on a notification.
class NotificationClickHandler : public base::CheckedObserver {
 public:
  ~NotificationClickHandler() override = default;
  // Called when the user clicks the PhoneHub notification which has a open
  // action.
  virtual void HandleNotificationClick(
      int64_t notification_id,
      const Notification::AppMetadata& app_metadata) = 0;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_
