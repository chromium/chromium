// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_popup_layout_impl.h"

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "ui/views/controls/separator.h"

BrowserViewPopupLayoutImpl::BrowserViewPopupLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayoutImpl(std::move(delegate), browser, std::move(views)) {
  // Some elements may be visible when they should not be. Remove them.
  if (this->views().tab_strip_region_view) {
    this->views().tab_strip_region_view->SetVisible(false);
  }
}

BrowserViewPopupLayoutImpl::~BrowserViewPopupLayoutImpl() = default;

gfx::Size BrowserViewPopupLayoutImpl::GetMinimumSize(
    const views::View* host) const {
  // The minimum size of a window is unrestricted for a borderless mode popup.
  if (delegate().GetBorderlessModeEnabled()) {
    return gfx::Size(1, 1);
  }

  const auto params =
      delegate().GetBrowserLayoutParams(/*use_browser_bounds=*/false);
  const auto leading = params.leading_exclusion.ContentWithPadding();
  const auto trailing = params.trailing_exclusion.ContentWithPadding();
  const gfx::Size caption_size(
      base::ClampCeil(leading.width() + trailing.width()),
      base::ClampCeil(std::max(leading.height(), trailing.height())));

  const gfx::Size toolbar_size =
      views().toolbar && views().toolbar->GetVisible()
          ? views().toolbar->GetMinimumSize()
          : gfx::Size();

  const int separator_height =
      views().top_container_separator &&
              views().top_container_separator->GetVisible()
          ? views::Separator::kThickness
          : 0;

  constexpr gfx::Size kMinContentsSize(1, 1);

  return gfx::Size(std::max({kMinContentsSize.width(), caption_size.width(),
                             toolbar_size.width()}),
                   kMinContentsSize.height() + caption_size.height() +
                       toolbar_size.height() + separator_height);
}

BrowserViewPopupLayoutImpl::ProposedLayout
BrowserViewPopupLayoutImpl::CalculateProposedLayout(
    const BrowserLayoutParams& browser_params) const {
  ProposedLayout layout;

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, browser_params.visual_client_area);
  }

  BrowserLayoutParams params = browser_params;

  // Lay out the top container if it's in the browser (even if it's empty).
  // This is required for certain things like hit-testing.
  if (IsParentedTo(views().top_container, views().browser_view)) {
    auto& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());
    const gfx::Rect top_container_local_bounds = CalculateTopContainerLayout(
        top_container_layout,
        params.InLocalCoordinates(params.visual_client_area),
        /*needs_exclusion=*/false);
    top_container_layout.bounds =
        GetTopContainerBoundsInParent(top_container_local_bounds, params);
    // In cases where the top container is nonzero size, need to move everything
    // else down.
    params.SetTop(std::max(params.visual_client_area.y(),
                           top_container_layout.bounds.bottom()));
  }

  // Lay out infobar container if present.
  if (IsParentedTo(views().infobar_container, views().browser_view)) {
    gfx::Rect infobar_bounds;
    const bool infobar_visible = delegate().IsInfobarVisible();
    if (infobar_visible) {
      infobar_bounds = gfx::Rect(
          params.visual_client_area.x(),
          // Infobar needs to get down out of the way of immersive mode elements
          // in some cases.
          params.visual_client_area.y() + delegate().GetExtraInfobarOffset(),
          params.visual_client_area.width(),
          // This returns zero for empty infobar.
          views().infobar_container->GetPreferredSize().height());
      params.SetTop(infobar_bounds.bottom());
    }
    layout.AddChild(views().infobar_container, infobar_bounds, infobar_visible);
  }

  // Lay out contents container.
  CHECK(
      IsParentedToAndVisible(views().contents_container, views().browser_view));
  gfx::Rect contents_bounds = params.visual_client_area;
  contents_bounds.set_height(std::max(contents_bounds.height(), 1));
  layout.AddChild(views().contents_container, contents_bounds);

  return layout;
}

gfx::Rect BrowserViewPopupLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    BrowserLayoutParams params,
    bool needs_exclusion) const {
  const int original_top = params.visual_client_area.y();

  // Layout starts beneath the caption buttons because title is laid out by the
  // OS in popup mode.
  params.SetTop(base::ClampCeil(
      std::max(params.leading_exclusion.ContentWithPadding().height(),
               params.trailing_exclusion.ContentWithPadding().height())));

  // Lay out the standard toolbar if present. This is used in tab fullscreen
  // when custom tabs are present.
  if (IsParentedTo(views().toolbar, views().top_container)) {
    const bool show_toolbar = delegate().IsToolbarVisible();
    gfx::Rect toolbar_bounds(params.visual_client_area.origin(), gfx::Size());
    if (show_toolbar) {
      toolbar_bounds.set_width(params.visual_client_area.width());
      toolbar_bounds.set_height(views().toolbar->GetPreferredSize().height());
    }
    layout.AddChild(views().toolbar, toolbar_bounds, show_toolbar);
    params.SetTop(toolbar_bounds.bottom());
  }

  // Add the top container separator. This is always present in popups.
  if (IsParentedTo(views().top_container_separator, views().top_container)) {
    const gfx::Rect separator_bounds(
        params.visual_client_area.x(), params.visual_client_area.y(),
        params.visual_client_area.width(), views::Separator::kThickness);
    layout.AddChild(views().top_container_separator, separator_bounds);
    params.SetTop(separator_bounds.bottom());
  }

  return gfx::Rect(params.visual_client_area.x(), original_top,
                   params.visual_client_area.width(),
                   params.visual_client_area.y() - original_top);
}
