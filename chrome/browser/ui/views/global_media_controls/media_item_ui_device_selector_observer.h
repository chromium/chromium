// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_OBSERVER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"

class DeviceEntryUI;

class MediaItemUIDeviceSelectorObserver : public base::CheckedObserver {
 public:
  // Called by MediaNotificationDeviceSelector view when available devices
  // changed.
  virtual void OnMediaItemUIDeviceSelectorUpdated(
      const std::map<int, raw_ptr<DeviceEntryUI, CtnExperimental>>&
          device_entries_map) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_OBSERVER_H_
