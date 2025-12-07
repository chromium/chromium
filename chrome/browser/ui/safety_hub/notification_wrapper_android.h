// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_WRAPPER_ANDROID_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_WRAPPER_ANDROID_H_

#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"

class NotificationWrapperAndroid
    : public RevokedPermissionsOSNotificationDisplayManager::
          SafetyHubNotificationWrapper {
 public:
  ~NotificationWrapperAndroid() override;
  void DisplayNotification(int num_revoked_permissions) override;
  void UpdateNotification(int num_revoked_permissions) override;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_WRAPPER_ANDROID_H_
