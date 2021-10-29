// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_

#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/notification.h"

namespace chromeos {
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
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash {
namespace phonehub {
using ::chromeos::phonehub::NotificationClickHandler;
}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_
