// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace global_media_controls {

class MediaItemUIUpdatedView;
class MediaItemUIView;

// A MediaItemUIDeviceSelector is a views::View that should be inserted into the
// bottom of the MediaItemUI which contains an expandable list of devices to
// connect to (audio/Cast/etc).
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIDeviceSelector
    : public views::View {
  METADATA_HEADER(MediaItemUIDeviceSelector, views::View)

 public:
  // Gives the device selector a pointer to the MediaItemUIView or
  // MediaItemUIUpdatedView so that it can inform it of device changes or size
  // changes. Only one of the MediaItemUIs should be set.
  virtual void SetMediaItemUIView(MediaItemUIView* view);
  virtual void SetMediaItemUIUpdatedView(MediaItemUIUpdatedView* view);

  // Called when the color theme has changed.
  virtual void OnColorsChanged(SkColor foreground, SkColor background) = 0;

  // Called when an audio device switch has occurred.
  virtual void UpdateCurrentAudioDevice(
      const std::string& current_device_id) = 0;

  // Called to show the device list since it is hidden before.
  virtual void ShowDevices() = 0;

  // Called to hide the device list since it is shown before.
  virtual void HideDevices() = 0;

  // Returns whether the device list has been expanded.
  virtual bool IsDeviceSelectorExpanded() = 0;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_
