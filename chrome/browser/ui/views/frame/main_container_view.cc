// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_container_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

MainContainerView::MainContainerView(BrowserView& browser_view)
    : browser_view_(browser_view) {}
MainContainerView::~MainContainerView() = default;

views::View::Views MainContainerView::GetChildrenInZOrder() {
  auto* top_container = browser_view_->top_container();
  if (top_container->parent() != this) {
    return views::View::GetChildrenInZOrder();
  }

  Views result = views::View::GetChildrenInZOrder();
  // Make sure `top_container_` is after `contents_container_` in paint order
  // when this is a window using WindowControlsOverlay, to make sure the window
  // controls are in fact drawn on top of the web contents.
  if (browser_view_->IsWindowControlsOverlayEnabled()) {
    auto top_container_iter = std::ranges::find(result, top_container);
    auto contents_container_iter =
        std::ranges::find(result, browser_view_->contents_container());
    CHECK(contents_container_iter != result.end());
    // Ensure `top_container_` paints after `contents_container_` when this is a
    // window using WindowControlsOverlay. This forces the window controls to be
    // painted on top of the web contents making them interactable to the user.
    // To do this, we rotate `result` in place.
    //
    // The rotation should only be done when not in Immersive Fullscreen since
    // that is the only time the top container and contents containers are
    // siblings to each other. However, during the transition there is a moment
    // where both could be true at the same time. This condition catches that
    // case as well.
    if (top_container_iter != result.end()) {
      std::rotate(top_container_iter, top_container_iter + 1,
                  contents_container_iter + 1);
    }

    return result;
  }

  return views::View::GetChildrenInZOrder();
}

BEGIN_METADATA(MainContainerView)
END_METADATA
