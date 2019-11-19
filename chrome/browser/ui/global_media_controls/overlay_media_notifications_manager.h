// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_H_

#include <map>
#include <memory>
#include <string>

class MediaNotificationService;
class OverlayMediaNotification;

// The OverlayMediaNotificationsManager owns, shows, and closes overlay media
// notifications. It keeps the MediaNotificationService informed of when the
// overlay notifications are closed.
class OverlayMediaNotificationsManager {
 public:
  explicit OverlayMediaNotificationsManager(MediaNotificationService* service);
  OverlayMediaNotificationsManager(const OverlayMediaNotificationsManager&) =
      delete;
  OverlayMediaNotificationsManager& operator=(
      const OverlayMediaNotificationsManager&) = delete;
  ~OverlayMediaNotificationsManager();

  // Displays the given OverlayMediaNotification.
  void ShowOverlayNotification(
      const std::string& id,
      std::unique_ptr<OverlayMediaNotification> overlay_notification);

  // Closes the OverlayMediaNotification with the given |id|.
  void CloseOverlayNotification(const std::string& id);

  // Called by the OverlayMediaNotification when the widget has closed.
  void OnOverlayNotificationClosed(const std::string& id);

 private:
  MediaNotificationService* const service_;
  std::map<std::string, std::unique_ptr<OverlayMediaNotification>>
      overlay_notifications_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_H_
