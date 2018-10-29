// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_NOTIFICATION_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_NOTIFICATION_CONTROLLER_H_

#include <string>

namespace proximity_auth {

// Responsible for displaying all notifications for EasyUnlock.
class NotificationController {
 public:
  NotificationController() {}
  virtual ~NotificationController() {}

  // Shows the notification when EasyUnlock is synced to a new Chromebook.
  virtual void ShowChromebookAddedNotification() = 0;

  // Shows the notification when EasyUnlock is already enabled on a Chromebook,
  // but a different phone is synced as the unlock key.
  virtual void ShowPairingChangeNotification() = 0;

  // Shows the notification after password reauth confirming that the new phone
  // should be used for EasyUnlock from now on.
  virtual void ShowPairingChangeAppliedNotification(
      const std::string& phone_name) = 0;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_NOTIFICATION_CONTROLLER_H_
