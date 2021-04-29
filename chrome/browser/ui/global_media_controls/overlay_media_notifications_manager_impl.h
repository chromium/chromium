// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager.h"

class MediaNotificationService;
class OverlayMediaNotification;

// The OverlayMediaNotificationsManagerImpl owns, shows, and closes overlay
// media notifications. It keeps the MediaNotificationService informed of when
// the overlay notifications are closed.
class OverlayMediaNotificationsManagerImpl
    : public OverlayMediaNotificationsManager {
 public:
  explicit OverlayMediaNotificationsManagerImpl(
      MediaNotificationService* service);
  OverlayMediaNotificationsManagerImpl(
      const OverlayMediaNotificationsManagerImpl&) = delete;
  OverlayMediaNotificationsManagerImpl& operator=(
      const OverlayMediaNotificationsManagerImpl&) = delete;
  ~OverlayMediaNotificationsManagerImpl();

  // Displays the given OverlayMediaNotification.
  void ShowOverlayNotification(
      const std::string& id,
      std::unique_ptr<OverlayMediaNotification> overlay_notification);

  // Closes the OverlayMediaNotification with the given |id|.
  void CloseOverlayNotification(const std::string& id);

  // OverlayMediaNotificationsManager override.
  void OnOverlayNotificationClosed(const std::string& id) override;

 private:
  MediaNotificationService* const service_;
  std::map<std::string, std::unique_ptr<OverlayMediaNotification>>
      overlay_notifications_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATIONS_MANAGER_IMPL_H_
