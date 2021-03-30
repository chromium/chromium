// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/metadata/metadata_impl_macros.h"

ThemeTrackingNonAccessibleImageView::ThemeTrackingNonAccessibleImageView(
    const gfx::ImageSkia& light_image,
    const gfx::ImageSkia& dark_image,
    const base::RepeatingCallback<SkColor()>& get_background_color_callback)
    : views::ThemeTrackingImageView(light_image,
                                    dark_image,
                                    get_background_color_callback) {}

ThemeTrackingNonAccessibleImageView::~ThemeTrackingNonAccessibleImageView() =
    default;

void ThemeTrackingNonAccessibleImageView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->AddState(ax::mojom::State::kInvisible);
}

BEGIN_METADATA(ThemeTrackingNonAccessibleImageView, ThemeTrackingImageView)
END_METADATA
