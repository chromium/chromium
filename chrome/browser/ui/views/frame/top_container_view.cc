// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/paint_info.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

TopContainerView::TopContainerView(BrowserView* browser_view)
    : browser_view_(browser_view) {
  SetProperty(views::kElementIdentifierKey, kTopContainerElementId);
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

void TopContainerView::OnPaintBackground(gfx::Canvas* canvas) {
  if (browser_view_->ShouldDrawVerticalTabStrip()) {
    // Top container draws an opaque background when in vertical tabstrip mode.

    // Rounded corners are drawn when not maximized or fullscreen.
    bool use_rounded_corners =
        !browser_view_->IsMaximized() && !browser_view_->IsFullscreen();
#if BUILDFLAG(IS_CHROMEOS)
    if (!chromeos::features::IsRoundedWindowsEnabled()) {
      use_rounded_corners = false;
    }
#endif
    gfx::ScopedCanvas scoped(canvas);
    if (use_rounded_corners) {
      const float radius = GetLayoutConstant(TOOLBAR_CORNER_RADIUS);
      const SkVector radii[4] = {
          {radius, radius}, {radius, radius}, {0, 0}, {0, 0}};
      const SkPath path = SkPath::RRect(
          SkRRect::MakeRectRadii(gfx::RectToSkRect(GetLocalBounds()), radii));
      canvas->ClipPath(path, true);
    }
    TopContainerBackground::PaintBackground(canvas, this, browser_view_);

  } else if (browser_view_->IsFullscreen()) {
    // When in immersive mode, top container is painted with the frame color.
    // The color matches the active frame, allowing the tabstrip to paint
    // correctly.
    canvas->DrawColor(GetColorProvider()->GetColor(ui::kColorFrameActive));
  }
}

void TopContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

BEGIN_METADATA(TopContainerView)
END_METADATA
