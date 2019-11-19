// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATION_VIEW_H_

#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "ui/views/widget/widget.h"

class MediaNotificationContainerImplView;

class OverlayMediaNotificationView : public OverlayMediaNotification,
                                     public views::Widget {
 public:
  OverlayMediaNotificationView(
      const std::string& id,
      std::unique_ptr<MediaNotificationContainerImplView> notification,
      gfx::Rect bounds);
  OverlayMediaNotificationView(const OverlayMediaNotificationView&) = delete;
  OverlayMediaNotificationView& operator=(const OverlayMediaNotificationView&) =
      delete;
  ~OverlayMediaNotificationView() override;

  // OverlayMediaNotification implementation.
  void SetManager(OverlayMediaNotificationsManager* manager) override;
  void ShowNotification() override;
  void CloseNotification() override;

  // views::Widget implementation.
  void OnNativeWidgetDestroyed() override;

 protected:
  OverlayMediaNotificationsManager* manager_ = nullptr;
  const std::string id_;
  MediaNotificationContainerImplView* const notification_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATION_VIEW_H_
