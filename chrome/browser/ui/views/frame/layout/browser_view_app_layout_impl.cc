// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_app_layout_impl.h"

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_types.h"

namespace {

views::LayoutAlignment GetWindowTitleAlignment() {
#if BUILDFLAG(IS_MAC)
  return views::LayoutAlignment::kCenter;
#else
  return views::LayoutAlignment::kStart;
#endif
}

// Ensure that the title isn't too close to the edge of the window. The logic
// for this may vary by platform. Only adjusts the region if it's closer to the
// edge than allowed.
void MaybeAdjustTitleRegionForWindowEdge(gfx::Rect& title_region,
                                         const BrowserLayoutParams& params) {
  // The minimum distance from the visual edge of the window.
  int from_edge = 0;
  // The minimum distance from the content of the exclusion area.
  int from_exclusion = 0;
#if BUILDFLAG(IS_MAC)
  // On Mac, this is determined by a constant percentage of window width.
  from_edge = base::ClampRound(params.visual_client_area.width() * 0.1);
#else
  // Match native Windows 10 UWP apps that don't have window icons.
  // TODO(crbug.com/40890502): Avoid hardcoding sizes like this.
  from_edge = 11;
  // This provides spacing next to the caption button or app icon even if there
  // is none
  from_exclusion = 5;
#endif

  int min_x = std::max(
      title_region.x(),
      params.visual_client_area.x() +
          std::max(from_edge,
                   base::ClampRound(params.leading_exclusion.content.width()) +
                       from_exclusion));
  int max_x = std::min(
      title_region.right(),
      params.visual_client_area.right() -
          std::max(from_edge,
                   base::ClampRound(params.trailing_exclusion.content.width()) +
                       from_exclusion));
  title_region.SetHorizontalBounds(min_x, max_x);
}

}  // namespace

BrowserViewAppLayoutImpl::BrowserViewAppLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayoutImpl(std::move(delegate), browser, std::move(views)) {}

BrowserViewAppLayoutImpl::~BrowserViewAppLayoutImpl() = default;

gfx::Size BrowserViewAppLayoutImpl::GetMinimumSize(
    const views::View* host) const {
  // The minimum size of a window is unrestricted for a borderless mode app.
  if (delegate().GetBorderlessModeEnabled()) {
    return gfx::Size(1, 1);
  }

  const gfx::Size title_size =
      views().web_app_window_title && views().web_app_window_title->GetVisible()
          ? views().web_app_window_title->GetMinimumSize()
          : gfx::Size();
  const gfx::Size web_app_toolbar_size =
      views().web_app_frame_toolbar &&
              views().web_app_frame_toolbar->GetVisible()
          ? views().web_app_frame_toolbar->GetMinimumSize()
          : gfx::Size();
  const gfx::Size tabstrip_size =
      views().tab_strip_region_view &&
              views().tab_strip_region_view->GetVisible()
          ? views().tab_strip_region_view->GetMinimumSize()
          : gfx::Size();
  const gfx::Size infobar_container_size =
      views().infobar_container->GetMinimumSize();
  gfx::Size contents_size = views().contents_container->GetMinimumSize();
  contents_size.SetToMin(gfx::Size(1, 1));

  const int width =
      std::max({web_app_toolbar_size.width() +
                    std::max(tabstrip_size.width(), title_size.width()),
                infobar_container_size.width()});
  const int height =
      std::max({title_size.height(), web_app_toolbar_size.height(),
                tabstrip_size.height()}) +
      infobar_container_size.height() + contents_size.height();
  return gfx::Size(width, height);
}

BrowserViewAppLayoutImpl::ProposedLayout
BrowserViewAppLayoutImpl::CalculateProposedLayout(
    const BrowserLayoutParams& browser_params) const {
  ProposedLayout layout;

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, browser_params.visual_client_area);
  }

  BrowserLayoutParams params = browser_params;

  // Maybe lay out the titlebar.
  if (IsParentedTo(views().web_app_frame_toolbar, views().browser_view)) {
    CalculateTitlebarLayout(layout, params);
  }

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

gfx::Rect BrowserViewAppLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    BrowserLayoutParams params,
    bool needs_exclusion) const {
  const int original_top = params.visual_client_area.y();

  if (IsParentedTo(views().web_app_frame_toolbar, views().top_container)) {
    CalculateTitlebarLayout(layout, params);
  }

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

  return gfx::Rect(params.visual_client_area.x(), original_top,
                   params.visual_client_area.width(),
                   params.visual_client_area.y() - original_top);
}

// Lays out titlebar elements, adding them to `layout`, and updating `params`.
// It is assumed that this handles the exclusions in `params`.
void BrowserViewAppLayoutImpl::CalculateTitlebarLayout(
    ProposedLayout& layout,
    BrowserLayoutParams& params) const {
  const bool should_draw_toolbar = delegate().ShouldDrawWebAppFrameToolbar();
  gfx::Rect full_toolbar_bounds =
      should_draw_toolbar
          ? GetBoundsWithExclusion(params, views().web_app_frame_toolbar)
          : gfx::Rect();
  const bool tabstrip_enabled =
      delegate().ShouldLayoutTabStrip() && delegate().ShouldDrawTabStrip();
  const bool overlay_controls_enabled =
      delegate().IsWindowControlsOverlayEnabled();
  CHECK(!tabstrip_enabled || !overlay_controls_enabled)
      << "Cannot enable both overlay and tabs at the same time.";
  if (tabstrip_enabled) {
    full_toolbar_bounds.Union(
        GetBoundsWithExclusion(params, views().tab_strip_region_view));
  }

  // Lay out the webapp toolbar.
  gfx::Rect toolbar_rect;
  if (should_draw_toolbar) {
    const int width =
        overlay_controls_enabled || tabstrip_enabled
            ? std::min(
                  full_toolbar_bounds.width(),
                  views().web_app_frame_toolbar->GetPreferredSize().width())
            : full_toolbar_bounds.width();

    // Overlay and tabstrip come before toolbar.
    toolbar_rect =
        gfx::Rect(full_toolbar_bounds.right() - width, full_toolbar_bounds.y(),
                  width, full_toolbar_bounds.height());
  }
  layout.AddChild(views().web_app_frame_toolbar, toolbar_rect,
                  should_draw_toolbar);

  // Lay out title.
  if (views().web_app_window_title) {
    const bool should_show_title =
        should_draw_toolbar && !overlay_controls_enabled && !tabstrip_enabled;
    gfx::Rect title_bounds;
    if (should_show_title) {
      gfx::Rect available =
          views().web_app_frame_toolbar->GetCenterContainerForSize(
              toolbar_rect.size());
      available.Offset(toolbar_rect.OffsetFromOrigin());
      MaybeAdjustTitleRegionForWindowEdge(available, params);
      switch (GetWindowTitleAlignment()) {
        case views::LayoutAlignment::kStart:
          title_bounds = available;
          break;
        case views::LayoutAlignment::kCenter: {
          const gfx::Size preferred =
              views().web_app_window_title->GetPreferredSize(
                  {available.width(), available.height()});
          title_bounds =
              gfx::Rect(params.visual_client_area.x(), available.y(),
                        params.visual_client_area.width(), available.height());
          title_bounds.ClampToCenteredSize(preferred);
          title_bounds.AdjustToFit(available);
          break;
        }
        default:
          NOTREACHED();
      }
    }
    layout.AddChild(views().web_app_window_title, title_bounds,
                    should_show_title);
  }

  // Lay out tabstrip if present.
  if (delegate().ShouldLayoutTabStrip()) {
    CHECK_EQ(views().tab_strip_region_view->parent(),
             views().web_app_frame_toolbar->parent())
        << "Always expect PWA toolbar and tabstrip to share the same "
           "coordinate basis.";
    const gfx::Rect tab_strip_bounds(
        full_toolbar_bounds.x(), full_toolbar_bounds.y(),
        full_toolbar_bounds.width() - toolbar_rect.width(),
        full_toolbar_bounds.height());
    layout.AddChild(views().tab_strip_region_view, tab_strip_bounds,
                    tabstrip_enabled);
  }

  // Update the overlay if present.
  if (overlay_controls_enabled && should_draw_toolbar) {
    // Unfortunately, the overlay is not a view in the same sense as the other
    // views and must be updated separately.
    overlay_rect_ =
        gfx::Rect(full_toolbar_bounds.x(), full_toolbar_bounds.y(),
                  full_toolbar_bounds.width() - toolbar_rect.width(),
                  full_toolbar_bounds.height());
  } else {
    // Move the available space downward when not in overlay mode (in overlay
    // mode the contents pane will need to render behind the overlay area).
    // Also, if the tabstrip is visible then it needs to be overlapped by the
    // contents slightly to give the impression that the tabs connect to the
    // contents.
    const int tabstrip_adjustment =
        tabstrip_enabled ? GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) : 0;
    params.SetTop(full_toolbar_bounds.bottom() - tabstrip_adjustment);
    overlay_rect_ = std::nullopt;
  }
}

void BrowserViewAppLayoutImpl::DoPostLayoutVisualAdjustments() {
  // Update the window controls overlay.
  if (overlay_rect_ && delegate().IsWindowControlsOverlayEnabled()) {
    delegate().UpdateWindowControlsOverlay(*overlay_rect_);
  }

  // Update the app title text label if necessary.
  // This logic is being moved out of the various browser frame implementations,
  // as layout of the titlebar is no longer delegated to the frame.
  if (views().web_app_window_title &&
      views().web_app_window_title->GetVisible()) {
    [[maybe_unused]] auto& label = *views().web_app_window_title;
#if BUILDFLAG(IS_MAC)
    // The background of the title area is always opaquely drawn, but when in
    // immersive fullscreen, it is drawn in a way that isn't detected by the
    // DCHECK in Label. As such, disable the DCHECK.
    label.SetSkipSubpixelRenderingOpacityCheck(
        ImmersiveModeController::From(browser())->IsEnabled());
#elif BUILDFLAG(IS_WIN)
    label.SetSubpixelRenderingEnabled(false);
    label.SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label.SetAutoColorReadabilityEnabled(false);
#elif BUILDFLAG(IS_LINUX)
    label.SetSubpixelRenderingEnabled(false);
    label.SetHorizontalAlignment(gfx::ALIGN_LEFT);
#endif
  }
}
