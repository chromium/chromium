// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_background_region_view.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

MainBackgroundRegionView::MainBackgroundRegionView(BrowserView& browser_view)
    : browser_view_(browser_view) {
  SetCanProcessEventsWithinSubtree(false);
  SetVisible(false);
}
MainBackgroundRegionView::~MainBackgroundRegionView() = default;

void MainBackgroundRegionView::Layout(PassKey) {
  if (ImmersiveModeController::From(browser_view_->browser())->IsEnabled()) {
    // Rounded top corners are not needed in immersive mode, so use an empty
    // clip path.
    SetClipPath(SkPathBuilder().detach());
  } else {
    const int corner_radius =
        GetLayoutConstant(MAIN_BACKGROUND_REGION_CORNER_RADIUS);

    // Clip path that outlines the main background region with rounded corners
    // at the top left and top right of the view.
    const SkPath path =
        SkPathBuilder()
            .moveTo(0, height())
            .lineTo(0, corner_radius)
            .arcTo(SkVector(corner_radius, corner_radius), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                   SkPoint(corner_radius, 0))
            .lineTo(width() - corner_radius, 0)
            .arcTo(SkVector(corner_radius, corner_radius), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                   SkPoint(width(), corner_radius))
            .lineTo(width(), height())
            .lineTo(0, height())
            .detach();
    SetClipPath(path);
  }
}

void MainBackgroundRegionView::OnPaint(gfx::Canvas* canvas) {
  TopContainerBackground::PaintBackground(canvas, this, &browser_view_.get());
}

BEGIN_METADATA(MainBackgroundRegionView) END_METADATA
