// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_container_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

MainContainerView::MainContainerView(BrowserView& browser_view)
    : browser_view_(browser_view) {}

MainContainerView::~MainContainerView() = default;

void MainContainerView::SetShadowVisiblityAndRoundedCorners(bool visible) {
  // No-op if visible set is the same as current state.
  if ((visible && layer()) || (!visible && !layer())) {
    return;
  }

  if (visible) {
    const int rounded_corner_radius =
        GetLayoutProvider()->GetCornerRadiusMetric(views::Emphasis::kHigh);
    const int elevation =
        GetLayoutProvider()->GetShadowElevationMetric(views::Emphasis::kHigh);

    SetPaintToLayer(ui::LAYER_TEXTURED);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(rounded_corner_radius));

    view_shadow_ = std::make_unique<views::ViewShadow>(this, elevation);
    view_shadow_->SetRoundedCornerRadius(rounded_corner_radius);
  } else {
    view_shadow_.reset();
    DestroyLayer();
  }
}

BEGIN_METADATA(MainContainerView)
END_METADATA
