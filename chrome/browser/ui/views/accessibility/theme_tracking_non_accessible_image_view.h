// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_THEME_TRACKING_NON_ACCESSIBLE_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_THEME_TRACKING_NON_ACCESSIBLE_IMAGE_VIEW_H_

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/theme_tracking_image_view.h"

// ImageView that sets the "invisible" state on AXNodeData so that
// the image is not traversed by screen readers. It also tracks the theme to
// show display either `light_image` or `dark_image`.
class ThemeTrackingNonAccessibleImageView
    : public views::ThemeTrackingImageView {
  METADATA_HEADER(ThemeTrackingNonAccessibleImageView,
                  views::ThemeTrackingImageView)

 public:
  ThemeTrackingNonAccessibleImageView(
      const ui::ImageModel& light_image_model,
      const ui::ImageModel& dark_image_model,
      const base::RepeatingCallback<SkColor()>& get_background_color_callback);
  // TODO(crbug.com/40239900): Remove this constructor and migrate existing
  // callers to `ImageModel`.
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
