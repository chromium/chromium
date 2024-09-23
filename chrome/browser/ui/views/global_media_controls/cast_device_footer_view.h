// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_FOOTER_VIEW_H_

#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/media_message_center/notification_theme.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Button;
class ImageView;
class Label;
class LabelButton;
}  // namespace views

// A footer view attached to MediaItemUIUpdatedView containing the casting
// device and a stop casting button for a cast media item. This is used within
// MediaDialogView on non-CrOS desktop platforms when the
// media::kGlobalMediaControlsUpdatedUI flag is enabled.
class CastDeviceFooterView : public global_media_controls::MediaItemUIFooter {
  METADATA_HEADER(CastDeviceFooterView,
                  global_media_controls::MediaItemUIFooter)

 public:
  explicit CastDeviceFooterView(
      std::optional<std::string> device_name,
      base::RepeatingClosure stop_casting_callback,
      media_message_center::MediaColorTheme media_color_theme);
  ~CastDeviceFooterView() override;

  // global_media_controls::MediaItemUIFooter:
  void OnColorsChanged(SkColor foreground, SkColor background) override {}

  // Helper functions for testing:
  views::Label* GetDeviceNameForTesting();
  views::Button* GetStopCastingButtonForTesting();

 private:
  void StopCasting();

  raw_ptr<views::ImageView> device_icon_;
  raw_ptr<views::Label> device_name_;
  raw_ptr<views::LabelButton> stop_casting_button_;

  base::RepeatingClosure stop_casting_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_FOOTER_VIEW_H_
