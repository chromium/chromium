// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/paint_info.h"
#include "ui/views/view_class_properties.h"

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
// `BrowserNonClientFrameViewChromeOS::Paint` here since it will be triggered by
// BrowserRootView::PaintChildren() on immersive revealed.
// TODO (b/287068468): Verify if it's needed on MacOS, once it's verified, we
// can decide whether keep or remove this function.
#if !BUILDFLAG(IS_CHROMEOS)
  if (browser_view_->immersive_mode_controller()->IsRevealed()) {
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
    browser_view_->frame()->GetFrameView()->Paint(
        views::PaintInfo::CreateRootPaintInfo(
            context, browser_view_->frame()->GetFrameView()->size()));
  }
#endif
  View::PaintChildren(paint_info);
}

void TopContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

BEGIN_METADATA(TopContainerView)
END_METADATA
