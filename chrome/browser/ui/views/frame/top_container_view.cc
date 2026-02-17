// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/paint_info.h"
#include "ui/views/view_class_properties.h"

TopContainerView::TopContainerView(BrowserView* browser_view)
    : browser_view_(browser_view) {
  SetProperty(views::kElementIdentifierKey, kTopContainerElementId);

  // Note: The colors will be set during layout, so these don't matter.
  auto background = std::make_unique<CustomCornersBackground>(
      *this, *browser_view, kColorToolbar, kColorToolbar);
  background->SetVisible(false);
  SetBackground(std::move(background));
}

TopContainerView::~TopContainerView() = default;

void TopContainerView::OnImmersiveRevealUpdated() {
  SchedulePaint();

  // TODO(crbug.com/41489962): Remove this once the View::SchedulePaint() API
  // has been updated to correctly invalidate layer-backed child views.
  for (auto& child : children()) {
    if (child->layer()) {
      child->layer()->SchedulePaint(
          ConvertRectToTarget(this, child, GetLocalBounds()));
    }
  }
}

bool TopContainerView::IsPositionInWindowCaption(
    const gfx::Point& test_point) const {
  const ToolbarView* const toolbar = browser_view_->toolbar();
  for (auto& child : children()) {
    gfx::Point logical_test_point(GetMirroredXInView(test_point.x()),
                                  test_point.y());
    if (child->GetVisible() && child->bounds().Contains(logical_test_point)) {
      if (child == toolbar) {
        const auto in_toolbar =
            views::View::ConvertPointToTarget(this, toolbar, test_point);
        if (toolbar->IsPositionInWindowCaption(in_toolbar)) {
          return true;
        }
      }
      return false;
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, the order of frame hit-testing is different, so if the area
  // with the caption buttons is not excluded here, it will override the actual
  // caption buttons with "frame", which in turn will prevent the buttons from
  // being used.
  const auto params =
      browser_view_->browser_widget()->GetFrameView()->GetBrowserLayoutParams();
  const gfx::Point in_browser =
      views::View::ConvertPointToTarget(this, browser_view_, test_point);
  const gfx::Rect caption_rect(
      browser_view_->width() - params.trailing_exclusion.content.width(), 0,
      params.trailing_exclusion.content.width(),
      params.trailing_exclusion.content.height());
  if (caption_rect.Contains(in_browser)) {
    return false;
  }
#endif

  return true;
}

void TopContainerView::PaintChildren(const views::PaintInfo& paint_info) {
// For ChromeOS, we don't need to manually call
// `BrowserFrameViewChromeOS::Paint` here since it will be triggered by
// BrowserRootView::PaintChildren() on immersive revealed.
// TODO (b/287068468): Verify if it's needed on MacOS, once it's verified, we
// can decide whether keep or remove this function.
#if !BUILDFLAG(IS_CHROMEOS)
  if (ImmersiveModeController::From(browser_view_->browser())->IsRevealed()) {
    // Top-views depend on parts of the frame (themes, window title, window
    // controls) being painted underneath them. Clip rect has already been set
    // to the bounds of this view, so just paint the frame.  Use a clone without
    // invalidation info, as we're painting something outside of the normal
    // parent-child relationship, so invalidations are no longer in the correct
    // space to compare.
    ui::PaintContext context(paint_info.context(),
                             ui::PaintContext::CLONE_WITHOUT_INVALIDATION);

    // Since TopContainerView is not an ancestor of BrowserFrameView, it is not
    // responsible for its painting. To call paint on BrowserFrameView, we need
    // to generate a new PaintInfo that shares a DisplayItemList with
    // TopContainerView.
    browser_view_->browser_widget()->GetFrameView()->Paint(
        views::PaintInfo::CreateRootPaintInfo(
            context, browser_view_->browser_widget()->GetFrameView()->size()));
  }
#endif
  View::PaintChildren(paint_info);
}

void TopContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

BEGIN_METADATA(TopContainerView)
END_METADATA
