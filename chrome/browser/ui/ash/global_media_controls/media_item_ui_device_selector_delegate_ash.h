// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_DELEGATE_ASH_H_
#define CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_DELEGATE_ASH_H_

#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"

// Used by MediaItemUIDeviceSelectorView for controlling audio devices,
// excluding Cast devices.
class MediaItemUIDeviceSelectorDelegateAsh
    : public MediaItemUIDeviceSelectorDelegate {
 public:
  MediaItemUIDeviceSelectorDelegateAsh() = default;
  ~MediaItemUIDeviceSelectorDelegateAsh() override = default;

  // MediaItemUIDeviceSelectorDelegate:
  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override;
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) override;
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback) override;
  void OnMediaRemotingRequested(const std::string& item_id) override;
};

#endif  // CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_DELEGATE_ASH_H_
