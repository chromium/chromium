// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_CAST_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_CAST_FOOTER_VIEW_H_

#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/media_message_center/notification_theme.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace {
class StopCastingButton;
}

// A footer view attached to MediaItemUIView containing a stop casting button
// for a cast media item. Currently only used on Chrome OS Ash.
class MediaItemUICastFooterView
    : public global_media_controls::MediaItemUIFooter {
  METADATA_HEADER(MediaItemUICastFooterView,
                  global_media_controls::MediaItemUIFooter)
 public:
  explicit MediaItemUICastFooterView(
      base::RepeatingClosure stop_casting_callback,
      media_message_center::MediaColorTheme media_color_theme);
  ~MediaItemUICastFooterView() override;

  // global_media_controls::MediaItemUIFooter:
  void OnColorsChanged(SkColor foreground, SkColor background) override {}

  views::Button* GetStopCastingButtonForTesting();

 private:
  void StopCasting();

  raw_ptr<StopCastingButton> stop_casting_button_;

  const base::RepeatingClosure stop_casting_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_CAST_FOOTER_VIEW_H_
