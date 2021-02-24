// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_THEME_TRACKING_NON_ACCESSIBLE_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_THEME_TRACKING_NON_ACCESSIBLE_IMAGE_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/theme_tracking_image_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

// ImageView that sets the "invisible" state on AXNodeData so that
// the image is not traversed by screen readers. It also tracks the theme to
// show display either |light_image| or |dark_image|.
class ThemeTrackingNonAccessibleImageView
    : public views::ThemeTrackingImageView {
 public:
  METADATA_HEADER(ThemeTrackingNonAccessibleImageView);
  ThemeTrackingNonAccessibleImageView(
      const gfx::ImageSkia& light_image,
      const gfx::ImageSkia& dark_image,
      const base::RepeatingCallback<SkColor()>& get_background_color_callback);
  ThemeTrackingNonAccessibleImageView(
      const ThemeTrackingNonAccessibleImageView&) = delete;
  ThemeTrackingNonAccessibleImageView& operator=(
      const ThemeTrackingNonAccessibleImageView&) = delete;
  ~ThemeTrackingNonAccessibleImageView() override;

 private:
  // Overridden from views::ImageView.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_THEME_TRACKING_NON_ACCESSIBLE_IMAGE_VIEW_H_
