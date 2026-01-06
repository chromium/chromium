// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_background_region_view.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/tabs/tab_strip_like_background.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"

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

void MainBackgroundRegionView::SetLeadingCornerVisible(
    bool leading_corner_visible) {
  if (leading_corner_visible_ == leading_corner_visible) {
    return;
  }
  leading_corner_visible_ = leading_corner_visible;
  InvalidateLayout(true);
}

void MainBackgroundRegionView::SetTrailingCornerVisible(
    bool trailing_corner_visible) {
  if (trailing_corner_visible_ == trailing_corner_visible) {
    return;
  }
  trailing_corner_visible_ = trailing_corner_visible;
  InvalidateLayout(true);
}

void MainBackgroundRegionView::Layout(PassKey) {
  background_view_->SetBoundsRect(GetLocalBounds());

  const int corner_radius =
      GetLayoutConstant(LayoutConstant::kMainBackgroundRegionCornerRadius);

  const int leading_corner_radius = leading_corner_visible_ ? corner_radius : 0;
  const int trailing_corner_radius =
      trailing_corner_visible_ ? corner_radius : 0;

  leading_corner_background_->SetBounds(0, 0, leading_corner_radius,
                                        leading_corner_radius);
  trailing_corner_background_->SetBounds(width() - trailing_corner_radius, 0,
                                         trailing_corner_radius,
                                         trailing_corner_radius);

  if (leading_corner_visible_ || trailing_corner_visible_) {
    // Clip path that outlines the main background region with rounded corners
    // at the top left and top right of the view.
    const SkPath path =
        SkPathBuilder()
            .moveTo(0, height())
            .lineTo(0, leading_corner_radius)
            .arcTo(SkVector(leading_corner_radius, leading_corner_radius), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                   SkPoint(leading_corner_radius, 0))
            .lineTo(width() - trailing_corner_radius, 0)
            .arcTo(SkVector(trailing_corner_radius, trailing_corner_radius), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                   SkPoint(width(), trailing_corner_radius))
            .lineTo(width(), height())
            .lineTo(0, height())
            .detach();
    background_view_->SetClipPath(path);
  } else {
    // Rounded top corners are not needed in various modes.
    background_view_->SetClipPath(SkPathBuilder().detach());
  }

  // Call super implementation to ensure layout manager and child layouts
  // happen.
  LayoutSuperclass<views::View>(this);
}

BEGIN_METADATA(MainBackgroundRegionView) END_METADATA
