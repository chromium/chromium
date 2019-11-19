// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager.h"

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"

OverlayMediaNotificationsManager::OverlayMediaNotificationsManager(
    MediaNotificationService* service)
    : service_(service) {
  DCHECK(service_);
}

OverlayMediaNotificationsManager::~OverlayMediaNotificationsManager() {
  overlay_notifications_.clear();
}

void OverlayMediaNotificationsManager::ShowOverlayNotification(
    const std::string& id,
    std::unique_ptr<OverlayMediaNotification> overlay_notification) {
  DCHECK(overlay_notification);
  OverlayMediaNotification* notification = overlay_notification.get();
  overlay_notifications_.insert({id, std::move(overlay_notification)});
  notification->SetManager(this);
  notification->ShowNotification();
}

void OverlayMediaNotificationsManager::CloseOverlayNotification(
    const std::string& id) {
  auto it = overlay_notifications_.find(id);
  if (it == overlay_notifications_.end())
    return;
  it->second->CloseNotification();
}

void OverlayMediaNotificationsManager::OnOverlayNotificationClosed(
    const std::string& id) {
  service_->OnOverlayNotificationClosed(id);

  // Warning: after this call, |id| is not safe to use since this deletes the
  // caller.
  overlay_notifications_.erase(id);
}
