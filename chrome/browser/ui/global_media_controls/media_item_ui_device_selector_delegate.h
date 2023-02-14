// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_DELEGATE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_DELEGATE_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"

class MediaItemUIDeviceSelectorDelegate {
 public:
  // Called when the user selects an audio device on the
  // `MediaItemUIDeviceSelectorView`.
  virtual void OnAudioSinkChosen(const std::string& id,
                                 const std::string& sink_id) = 0;

  // Used by a `MediaItemUIDeviceSelectorView` to query the system for connected
  // audio output devices.
  virtual base::CallbackListSubscription
  RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) = 0;

  // Used by a `MediaItemUIDeviceSelectorView` to become notified of audio
  // device switching capabilities. The callback will be immediately run with
  // the current availability.
  virtual base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback) = 0;

  // Used by `MediaItemUIDeviceSelectorView` to send commands to MediaController
  // to request starting a Remote Playback session.
  virtual void OnMediaRemotingRequested(const std::string& item_id) = 0;

 protected:
  virtual ~MediaItemUIDeviceSelectorDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_DELEGATE_H_
