// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_H_

#include "base/callback_list.h"
#include "media/audio/audio_system.h"

class MediaNotificationDeviceProvider {
 public:
  virtual ~MediaNotificationDeviceProvider() = default;

  using GetOutputDevicesCallbackList =
      base::RepeatingCallbackList<void(const media::AudioDeviceDescriptions&)>;
  using GetOutputDevicesCallback = GetOutputDevicesCallbackList::CallbackType;

  // Register a callback that will be invoked with the list of audio output
  // devices currently available. After the first invocation the callback will
  // be run whenever a change in connected audio devices is detected.
  virtual base::CallbackListSubscription
  RegisterOutputDeviceDescriptionsCallback(GetOutputDevicesCallback cb) = 0;

  // Query the system for audio output devices and reply via callback.
  virtual void GetOutputDeviceDescriptions(
      media::AudioSystem::OnDeviceDescriptionsCallback) = 0;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_H_
