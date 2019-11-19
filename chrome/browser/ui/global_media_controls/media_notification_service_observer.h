// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

class MediaNotificationServiceObserver : public base::CheckedObserver {
 public:
  // Called when the list of active, cast, or frozen media notifications
  // changes.
  virtual void OnNotificationListChanged() = 0;

  // Called when a media dialog associated with the service is either opened or
  // closed.
  virtual void OnMediaDialogOpenedOrClosed() = 0;

 protected:
  ~MediaNotificationServiceObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_OBSERVER_H_
