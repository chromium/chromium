// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_FOOTER_VIEW_H_

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace {
class DeviceEntryButton;
}  // anonymous namespace

// A footer view attached to media_notification_view_impl containing
// available cast devices and volume controls.
class MediaNotificationFooterView
    : public views::View,
      public MediaNotificationDeviceSelectorObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnDeviceSelected(int tag) = 0;
    virtual void OnDropdownButtonClicked() = 0;
    virtual bool IsDeviceSelectorExpanded() = 0;
  };

  MediaNotificationFooterView(
      bool is_cast_session,
      views::Button::PressedCallback stop_casting_callback);
  ~MediaNotificationFooterView() override = default;

  void OnColorChanged(SkColor foreground);
  void SetDelegate(Delegate* delegate);

  // MediaNotificationDeviceselectorobserver
  void OnMediaNotificationDeviceSelectorUpdated(
      const std::map<int, DeviceEntryUI*>& device_entries_map) override;

  void Layout() override;

 private:
  void UpdateButtonsColor();
  void OnDeviceSelected(int tag);
  void OnOverflowButtonClicked();

  SkColor foreground_color_ = gfx::kPlaceholderColor;

  DeviceEntryButton* overflow_button_ = nullptr;

  Delegate* delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_FOOTER_VIEW_H_
