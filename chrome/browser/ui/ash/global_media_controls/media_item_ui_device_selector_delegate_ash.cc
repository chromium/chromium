// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/global_media_controls/media_item_ui_device_selector_delegate_ash.h"

#include "base/notreached.h"

void MediaItemUIDeviceSelectorDelegateAsh::OnAudioSinkChosen(
    const std::string& id,
    const std::string& sink_id) {
  // TODO(crbug.com/40261717): Implement this.
  NOTIMPLEMENTED();
}

base::CallbackListSubscription MediaItemUIDeviceSelectorDelegateAsh::
    RegisterAudioOutputDeviceDescriptionsCallback(
        MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
            CallbackType callback) {
  // TODO(crbug.com/40261717): Implement this.
  NOTIMPLEMENTED();
  return base::CallbackListSubscription();
}

base::CallbackListSubscription MediaItemUIDeviceSelectorDelegateAsh::
    RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
        const std::string& id,
        base::RepeatingCallback<void(bool)> callback) {
  // TODO(crbug.com/40261717): Implement this.
  NOTIMPLEMENTED();
  return base::CallbackListSubscription();
}

void MediaItemUIDeviceSelectorDelegateAsh::OnMediaRemotingRequested(
    const std::string& item_id) {
  // TODO(crbug.com/40261717): Implement this.
  NOTIMPLEMENTED();
}
