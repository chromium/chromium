// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_

#include "base/observer_list.h"

namespace chromeos {
namespace phonehub {

// Handles actions performed on a notification.
class NotificationClickHandler : public base::CheckedObserver {
 public:
  ~NotificationClickHandler() override = default;
  // Called when the user clicks the PhoneHub notification which has a open
  // action.
  virtual void HandleNotificationClick(int64_t notification_id) = 0;
};

}  // namespace phonehub
}  // namespace chromeos
#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_CLICK_HANDLER_H_
