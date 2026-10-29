// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"

ThemeTrackingNonAccessibleImageView::ThemeTrackingNonAccessibleImageView(
    const ui::ImageModel& light_image_model,
    const ui::ImageModel& dark_image_model,
    ui::ColorVariant background_color)
    : views::ThemeTrackingImageView(light_image_model,
                                    dark_image_model,
                                    background_color) {
  GetViewAccessibility().SetIsInvisible(true);
}

ThemeTrackingNonAccessibleImageView::ThemeTrackingNonAccessibleImageView(
    const gfx::ImageSkia& light_image,
    const gfx::ImageSkia& dark_image,
    ui::ColorVariant background_color)
    : views::ThemeTrackingImageView(light_image, dark_image, background_color) {
  GetViewAccessibility().SetIsInvisible(true);
}

ThemeTrackingNonAccessibleImageView::~ThemeTrackingNonAccessibleImageView() =
    default;

BEGIN_METADATA(ThemeTrackingNonAccessibleImageView)
END_METADATA
