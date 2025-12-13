// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_background_region_view.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tabs/tab_strip_like_background.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

MainBackgroundRegionView::MainBackgroundRegionView(BrowserView& browser_view)
    : browser_view_(browser_view) {
  SetCanProcessEventsWithinSubtree(false);
  SetVisible(false);

  leading_corner_background_ = AddChildView(std::make_unique<views::View>());
  leading_corner_background_->SetBackground(
      std::make_unique<TabStripLikeBackground>(&browser_view_.get()));
  trailing_corner_background_ = AddChildView(std::make_unique<views::View>());
  trailing_corner_background_->SetBackground(
      std::make_unique<TabStripLikeBackground>(&browser_view_.get()));
  background_view_ = AddChildView(std::make_unique<views::View>());
  background_view_->SetBackground(
      std::make_unique<TopContainerBackground>(&browser_view_.get()));
}
MainBackgroundRegionView::~MainBackgroundRegionView() = default;

void MainBackgroundRegionView::Layout(PassKey) {
  background_view_->SetBoundsRect(GetLocalBounds());
  if (ImmersiveModeController::From(browser_view_->browser())->IsEnabled()) {
    // Rounded top corners are not needed in immersive mode, so use an empty
    // clip path.
    background_view_->SetClipPath(SkPathBuilder().detach());
  } else {
    const int corner_radius =
        GetLayoutConstant(MAIN_BACKGROUND_REGION_CORNER_RADIUS);

    leading_corner_background_->SetBounds(0, 0, corner_radius, corner_radius);
    trailing_corner_background_->SetBounds(width() - corner_radius, 0,
                                           corner_radius, corner_radius);

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
    background_view_->SetClipPath(path);
  }

  // Call super implementation to ensure layout manager and child layouts
  // happen.
  LayoutSuperclass<views::View>(this);
}

BEGIN_METADATA(MainBackgroundRegionView) END_METADATA
