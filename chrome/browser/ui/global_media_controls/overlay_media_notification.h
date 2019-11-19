// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATION_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATION_H_

class OverlayMediaNotificationsManager;

// Handles displaying media notifications as overlay controls.
class OverlayMediaNotification {
 public:
  // OverlayMediaNotification is owned and destroyed by the
  // OverlayMediaNotificationsManager.
  virtual ~OverlayMediaNotification() = default;

  // Sets the OverlayMediaNotificationsManager associated with this
  // OverlayMediaNotification.
  virtual void SetManager(OverlayMediaNotificationsManager* manager) = 0;

  // Displays the widget. |SetManager()| must be called first the ensure that
  // the manager is set.
  virtual void ShowNotification() = 0;

  // Closes the widget.
  virtual void CloseNotification() = 0;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_OVERLAY_MEDIA_NOTIFICATION_H_
