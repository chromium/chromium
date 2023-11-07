// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_LEGACY_CAST_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_LEGACY_CAST_FOOTER_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"

namespace views {
class Button;
class LabelButton;
}  // namespace views

// A footer view attached to MediaItemUIView for Cast items when modern UI is
// disabled.
class MediaItemUILegacyCastFooterView
    : public global_media_controls::MediaItemUIFooter {
  METADATA_HEADER(MediaItemUILegacyCastFooterView,
                  global_media_controls::MediaItemUIFooter)
 public:
  explicit MediaItemUILegacyCastFooterView(
      base::RepeatingClosure stop_casting_callback);
  ~MediaItemUILegacyCastFooterView() override;

  // global_media_controls::MediaItemUIFooter:
  void OnColorsChanged(SkColor foreground, SkColor background) override;

  views::Button* GetStopCastingButtonForTesting();

 private:
  void StopCasting();

  void UpdateColors();

  SkColor foreground_color_ = global_media_controls::kDefaultForegroundColor;
  SkColor background_color_ = global_media_controls::kDefaultBackgroundColor;

  raw_ptr<views::LabelButton> stop_cast_button_ = nullptr;

  const base::RepeatingClosure stop_casting_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_LEGACY_CAST_FOOTER_VIEW_H_
