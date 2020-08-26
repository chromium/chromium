// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_DELEGATE_H_

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"

class MediaNotificationAudioDeviceSelectorViewDelegate {
 public:
  virtual void OnAudioSinkChosen(const std::string& sink_id) = 0;
  virtual void OnAudioDeviceSelectorViewSizeChanged() = 0;
  virtual std::unique_ptr<MediaNotificationDeviceProvider::
                              GetOutputDevicesCallbackList::Subscription>
  RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_AUDIO_DEVICE_SELECTOR_VIEW_DELEGATE_H_
