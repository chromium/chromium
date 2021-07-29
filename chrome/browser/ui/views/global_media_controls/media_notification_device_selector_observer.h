// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_OBSERVER_H_

#include <map>

#include "base/observer_list_types.h"

class DeviceEntryUI;

class MediaNotificationDeviceSelectorObserver : public base::CheckedObserver {
 public:
  // Called by MediaNotificationDeviceSelector view when available devices
  // changed.
  virtual void OnMediaNotificationDeviceSelectorUpdated(
      const std::map<int, DeviceEntryUI*>& device_entries_map) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_OBSERVER_H_
