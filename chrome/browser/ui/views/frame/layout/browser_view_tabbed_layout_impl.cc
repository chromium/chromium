// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "chrome/browser/ui/views/frame/main_background_region_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/separator.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// Loading bar is thicker than a separator, but instead of moving the bottom
// of the top container down, it starts above where the separator would go.
constexpr int kLoadingBarHeight = 3;
constexpr int kLoadingBarOffset =
    kLoadingBarHeight - views::Separator::kThickness;

// Minimum area next to caption buttons to use as a grab handle.
constexpr int kVerticalTabsGrabHandleSize = 54;

// Maximum portion of the window a "size-restricted" contents-height side panel
// can take up. This is not the only limit on side panel size.
constexpr float kMaxContentsHeightSidePanelFraction = 2.f / 3.f;

// Increases the leading or trailing exclusion padding to `minimum`.
void IncreasePaddingToMinimum(BrowserLayoutParams& params, int minimum) {
  if (params.leading_exclusion.content.width() > 0.0) {
    params.leading_exclusion.horizontal_padding =
        std::max(params.leading_exclusion.horizontal_padding,
                 static_cast<float>(minimum));
  } else {
    params.trailing_exclusion.horizontal_padding =
        std::max(params.trailing_exclusion.horizontal_padding,
                 static_cast<float>(minimum));
  }
}

// Gets the sum of leading and trailing exclusions in `params` for minimum-size
// computation.
int GetExclusionWidth(const BrowserLayoutParams& params) {
  const float width = params.leading_exclusion.content.width() +
                      params.trailing_exclusion.content.width();
  const float padding = params.leading_exclusion.horizontal_padding +
                        params.trailing_exclusion.horizontal_padding;
  return base::ClampCeil(width + padding);
}

}  // namespace

BrowserViewTabbedLayoutImpl::BrowserViewTabbedLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayoutImpl(std::move(delegate), browser, std::move(views)) {}

BrowserViewTabbedLayoutImpl::~BrowserViewTabbedLayoutImpl() = default;

BrowserViewTabbedLayoutImpl::TopSeparatorType
BrowserViewTabbedLayoutImpl::GetTopSeparatorType() const {
  if (!delegate().IsToolbarVisible() && !delegate().IsBookmarkBarVisible()) {
    return TopSeparatorType::kNone;
  }

  if (IsParentedTo(views().loading_bar, views().top_container)) {
    return TopSeparatorType::kLoadingBar;
  }

  // In immersive mode, when the top container is visually separate, the
  // separator goes with the container to the overlay.
  bool top_container_is_visually_separate =
      delegate().GetBrowserWindowState() == WindowState::kFullscreen;
#if BUILDFLAG(IS_MAC)
  // On Mac, when in full browser fullscreen (but not content fullscreen), the
  // entire top container is always visible and does not look like an
  // immersive mode overlay, so in this case the top container isn't visually
  // separate from the browser.
  if (top_container_is_visually_separate &&
      fullscreen_utils::IsAlwaysShowToolbarEnabled(browser()) &&
      !fullscreen_utils::IsInContentFullscreen(browser())) {
    // If there is a shadow box, it serves as a separator, so none is needed.
    if (ShadowOverlayVisible()) {
      return TopSeparatorType::kNone;
    }
    top_container_is_visually_separate = false;
  }
#endif
  if (top_container_is_visually_separate) {
    return TopSeparatorType::kTopContainer;
  }

  // If the infobar is visible, the separator has to go in the top container.
  if (delegate().IsInfobarVisible()) {
    return TopSeparatorType::kTopContainer;
  }

  // The separator should go in the multi contents view instead.
  return TopSeparatorType::kMultiContents;
}

std::pair<gfx::Size, gfx::Size>
BrowserViewTabbedLayoutImpl::GetMinimumTabStripSize(
    const BrowserLayoutParams& params) const {
  switch (GetTabStripType()) {
    case TabStripType::kHorizontal: {
      auto result = views().horizontal_tab_strip_region_view->GetMinimumSize();
      result.Enlarge(GetExclusionWidth(params), 0);
      return std::make_pair(gfx::Size(), result);
    }
    case TabStripType::kVertical: {
      auto result = views().vertical_tab_strip_region_view->GetMinimumSize();
      if (GetVerticalTabStripCollapsedState() !=
          VerticalTabStripCollapsedState::kCollapsed) {
        result.set_width(std::max(
            result.width(),
            base::ClampCeil(
                params.leading_exclusion.ContentWithPadding().width())));
      }
      return std::make_pair(result, gfx::Size());
    }
    case TabStripType::kWebUi:
      // WebUI tabstrip is lazily-created.
      return std::make_pair(gfx::Size(),
                            views().webui_tab_strip
                                ? views().webui_tab_strip->GetMinimumSize()
                                : gfx::Size());
    case TabStripType::kNone:
      return std::make_pair(gfx::Size(), gfx::Size());
  }
}

BrowserViewTabbedLayoutImpl::HorizontalLayout
BrowserViewTabbedLayoutImpl::CalculateHorizontalLayout(
    BrowserLayoutParams& params) const {
  HorizontalLayout layout;
  const auto tab_strip_type = GetTabStripType();

  // Start with some preliminary values.
  layout.force_top_container_to_top =
      tab_strip_type != TabStripType::kHorizontal;
  layout.min_content_width = kContentsContainerMinimumWidth;

  // Get information about the vertical tabstrip, if present.
  const int toolbar_minimum_width = views().toolbar->GetMinimumSize().width();
  int min_vertical_tab_strip_width = 0;
  int preferred_vertical_tab_strip_width = 0;
  if (tab_strip_type == TabStripType::kVertical) {
    const auto* const vertical_tab_strip =
        views().vertical_tab_strip_region_view.get();
    preferred_vertical_tab_strip_width =
        vertical_tab_strip->GetPreferredSize().width();
    layout.vertical_tab_strip_collapsed_state =
        GetVerticalTabStripCollapsedState();
    if (layout.vertical_tab_strip_collapsed_state ==
        VerticalTabStripCollapsedState::kCollapsed) {
      // Collapsed tab strip always gets its preferred size.
      min_vertical_tab_strip_width = preferred_vertical_tab_strip_width;
    } else {
      // Minimum size is bounded from below by size of leading exclusion area.
      if (layout.vertical_tab_strip_collapsed_state ==
          VerticalTabStripCollapsedState::kExpanded) {
        min_vertical_tab_strip_width = std::max(
            vertical_tab_strip->GetMinimumSize().width(),
            base::ClampCeil(
                params.leading_exclusion.ContentWithPadding().width()));
      }

      // Figure out the maximum size of the vertical tabstrip that can still
      // accommodate the toolbar.
      //
      // Subtract everything that must go to the right of the vertical tabstrip
      // from the available width.
      const int remainder =
          params.visual_client_area.width() - toolbar_minimum_width -
          base::ClampCeil(
              params.trailing_exclusion.ContentWithPadding().width());
      preferred_vertical_tab_strip_width =
          std::max(min_vertical_tab_strip_width,
                   std::min(remainder, preferred_vertical_tab_strip_width));
    }

    // Account for grab handle.
    IncreasePaddingToMinimum(params, kVerticalTabsGrabHandleSize);
  }

  // Get information about the toolbar-height side panel, if present.
  int min_toolbar_height_side_panel_width = 0;
  int preferred_toolbar_height_side_panel_width = 0;
  if (const auto* const panel = views().toolbar_height_side_panel.get();
      IsParentedToAndVisible(panel, views().browser_view)) {
    min_toolbar_height_side_panel_width = panel->GetMinimumSize().width();
    preferred_toolbar_height_side_panel_width = std::max(
        min_toolbar_height_side_panel_width, panel->GetPreferredSize().width());
    layout.side_panel_padding =
        GetLayoutConstant(LayoutConstant::kToolbarHeightSidePanelInset);

    // See if the toolbar-height side panel can fit next to the toolbar. If not,
    // it is forced into content height.
    if (!layout.force_top_container_to_top) {
      const int remainder = params.visual_client_area.width() -
                            (toolbar_minimum_width + layout.side_panel_padding);
      layout.force_top_container_to_top =
          remainder < min_toolbar_height_side_panel_width;
    }
  }

  // Get information about the content-height side panel, if present.
  int min_content_height_side_panel_width = 0;
  int preferred_content_height_side_panel_width = 0;
  if (const auto* const panel = views().contents_height_side_panel.get();
      IsParentedToAndVisible(panel, views().browser_view)) {
    min_content_height_side_panel_width = panel->GetMinimumSize().width();
    preferred_content_height_side_panel_width = std::max(
        min_content_height_side_panel_width, panel->GetPreferredSize().width());

    // Some panels impose a second limit on size.
    if (panel->ShouldRestrictMaxWidth()) {
      preferred_content_height_side_panel_width =
          std::min(preferred_content_height_side_panel_width,
                   base::ClampFloor(params.visual_client_area.width() *
                                    kMaxContentsHeightSidePanelFraction));
    }

    // Side panel implies a separator, which means we have to give a little
    // more room for the contents.
    layout.min_content_width += views::Separator::kThickness;
  }

  // When both side panels are present, one is animating in and the other is
  // animating out. Give precedence to the toolbar-height panel.
  const bool use_toolbar_height = preferred_toolbar_height_side_panel_width > 0;

  // Start with the minimum values for each element.
  layout.vertical_tab_strip_width = min_vertical_tab_strip_width;
  layout.toolbar_height_side_panel_width = min_toolbar_height_side_panel_width;
  layout.content_height_side_panel_width = min_content_height_side_panel_width;

  // Determine how much space is left to allocate.
  int remaining = params.visual_client_area.width() - layout.min_content_width -
                  layout.side_panel_padding - min_vertical_tab_strip_width -
                  (use_toolbar_height ? min_toolbar_height_side_panel_width
                                      : min_content_height_side_panel_width);

  // Keep the width of the tabstrip stable when possible; allocate as much space
  // as possible to it (up to its maximum).
  if (remaining > 0) {
    const int vertical_tab_strip_flex_amount =
        std::min(remaining, preferred_vertical_tab_strip_width -
                                min_vertical_tab_strip_width);
    layout.vertical_tab_strip_width += vertical_tab_strip_flex_amount;
    remaining -= vertical_tab_strip_flex_amount;
  }

  // Side panels won't both be fully present at the same time, so allocate the
  // rest of the remainder to both.
  if (remaining > 0) {
    const int toolbar_height_flex_amount =
        std::min(remaining, preferred_toolbar_height_side_panel_width -
                                min_toolbar_height_side_panel_width);
    layout.toolbar_height_side_panel_width += toolbar_height_flex_amount;
    const int content_height_flex_amount =
        std::min(remaining, preferred_content_height_side_panel_width -
                                min_content_height_side_panel_width);
    layout.content_height_side_panel_width += content_height_flex_amount;
  }

  return layout;
}

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumMainAreaSize(
    const BrowserLayoutParams& params) const {
  gfx::Size toolbar_size = views().toolbar->GetMinimumSize();
  if (GetTabStripType() == TabStripType::kVertical) {
    toolbar_size.Enlarge(GetExclusionWidth(params), 0);
  }
  const gfx::Size bookmark_bar_size =
      (views().bookmark_bar && views().bookmark_bar->GetVisible())
          ? views().bookmark_bar->GetMinimumSize()
          : gfx::Size();
  const gfx::Size infobar_container_size =
      views().infobar_container->GetMinimumSize();
  const gfx::Size contents_size = views().contents_container->GetMinimumSize();
  const gfx::Size contents_height_side_panel_size =
      views().contents_height_side_panel &&
              views().contents_height_side_panel->GetVisible()
          ? views().contents_height_side_panel->GetMinimumSize()
          : gfx::Size();

  const int width = std::max({toolbar_size.width(), bookmark_bar_size.width(),
                              infobar_container_size.width(),
                              contents_height_side_panel_size.width() +
                                  kContentsContainerMinimumWidth});
  const int height = toolbar_size.height() + bookmark_bar_size.height() +
                     infobar_container_size.height() +
                     std::max(contents_size.height(),
                              contents_height_side_panel_size.height());
  return gfx::Size(width, height);
}

BrowserViewTabbedLayoutImpl::TabStripType
BrowserViewTabbedLayoutImpl::GetTabStripType() const {
  if (delegate().ShouldUseTouchableTabstrip()) {
    return TabStripType::kWebUi;
  }
  if (delegate().ShouldDrawVerticalTabStrip()) {
    return TabStripType::kVertical;
  }
  return delegate().ShouldDrawTabStrip() ? TabStripType::kHorizontal
                                         : TabStripType::kNone;
}

bool BrowserViewTabbedLayoutImpl::ShadowOverlayVisible() const {
  if (!views().toolbar_height_side_panel) {
    return false;
  }
  return views().toolbar_height_side_panel->GetVisible();
}

BrowserViewTabbedLayoutImpl::VerticalTabStripCollapsedState
BrowserViewTabbedLayoutImpl::GetVerticalTabStripCollapsedState() const {
  if (!views().vertical_tab_strip_region_view) {
    return VerticalTabStripCollapsedState::kExpanded;
  }
  const auto percent =
      views().vertical_tab_strip_region_view->GetCollapseAnimationPercent();
  const bool is_collapsed = delegate().IsVerticalTabStripCollapsed();
  if (is_collapsed) {
    return percent ? VerticalTabStripCollapsedState::kCollapsing
                   : VerticalTabStripCollapsedState::kCollapsed;
  }
  return percent ? VerticalTabStripCollapsedState::kExpanding
                 : VerticalTabStripCollapsedState::kExpanded;
}

int BrowserViewTabbedLayoutImpl::GetCollapsedVerticalTabStripRelativeTop(
    const BrowserLayoutParams& params) const {
  // When the top container isn't in the browser view, the exclusion won't apply
  // and the tabstrip goes all the way to the top.
  if (!IsParentedTo(views().top_container, views().browser_view)) {
    return 0;
  }

  // If there is no leading exclusion, the tabstrip goes all the way to the top.
  if (params.leading_exclusion.IsEmpty()) {
    return 0;
  }

  const int exclusion_height =
      base::ClampCeil(params.leading_exclusion.ContentWithPadding().height());

  // Try to align with toolbar. But if it's not visible, then don't.
  if (!delegate().IsToolbarVisible()) {
    return exclusion_height;
  }

  // Gets where the bottom of the toolbar will be laid out.
  const int provisional_toolbar_height =
      GetBoundsWithExclusion(params, views().toolbar).height();
  return std::max(exclusion_height, provisional_toolbar_height);
}

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumSize(
    const views::View* host) const {
  auto params = delegate().GetBrowserLayoutParams(/*use_browser_bounds=*/true);

  // This is a simplified version of the same method in
  // `BrowserViewLayoutImplOld` that assumes a standard browser.
  const auto [vertical_tabstrip_size, horizontal_tabstrip_size] =
      GetMinimumTabStripSize(params);
  if (!vertical_tabstrip_size.IsEmpty()) {
    IncreasePaddingToMinimum(params, kVerticalTabsGrabHandleSize);
  }
  params.InsetHorizontal(vertical_tabstrip_size.width(), /*leading=*/true);
  const gfx::Size toolbar_height_side_panel_size =
      views().toolbar_height_side_panel &&
              views().toolbar_height_side_panel->GetVisible()
          ? views().toolbar_height_side_panel->GetMinimumSize()
          : gfx::Size();
  const gfx::Size main_area_size = GetMinimumMainAreaSize(params);

  int min_height =
      horizontal_tabstrip_size.height() +
      std::max({toolbar_height_side_panel_size.height(),
                main_area_size.height(), vertical_tabstrip_size.height()});

  // This assumes a horizontal tabstrip. There is also a hard minimum on the
  // width of the browser defined by `kMainBrowserContentsMinimumWidth`.
  int min_width =
      vertical_tabstrip_size.width() +
      std::max({horizontal_tabstrip_size.width(),
                toolbar_height_side_panel_size.width() + main_area_size.width(),
                kMainBrowserContentsMinimumWidth});

  // Maybe adjust for additional padding when toolbar height side panel is
  // visible.
  if (!toolbar_height_side_panel_size.IsEmpty()) {
    const auto padding =
        GetLayoutConstant(LayoutConstant::kToolbarHeightSidePanelInset);
    min_height += 2 * padding;
    min_width += padding;
  }

  return gfx::Size(min_width, min_height);
}

BrowserViewTabbedLayoutImpl::ProposedLayout
BrowserViewTabbedLayoutImpl::CalculateProposedLayout(
    const BrowserLayoutParams& browser_params) const {
  // TODO(https://crbug.com/453717426): Consider caching layouts of the same
  // size if no `InvalidateLayout()` has happened.

  // Build the proposed layout here:
  TRACE_EVENT0("ui", "BrowserViewLayoutImpl::CalculateProposedLayout");
  ProposedLayout layout;

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, browser_params.visual_client_area);
  }

  BrowserLayoutParams params = browser_params;
  bool needs_exclusion = true;
  const TabStripType tab_strip_type = GetTabStripType();
  HorizontalLayout horizontal_layout = CalculateHorizontalLayout(params);

  if (tab_strip_type == TabStripType::kWebUi) {
    // When the WebUI tab strip is present, it does not paint over the caption
    // buttons or other exclusion areas.
    params.SetTop(base::ClampCeil(
        std::max(params.leading_exclusion.ContentWithPadding().height(),
                 params.trailing_exclusion.ContentWithPadding().height())));
    needs_exclusion = false;
  }

  // Lay out WebUI tabstrip if visible.
  if (IsParentedTo(views().webui_tab_strip, views().browser_view)) {
    const int width = params.visual_client_area.width();
    const int height = tab_strip_type == TabStripType::kWebUi &&
                               views().webui_tab_strip->GetVisible()
                           ? views().webui_tab_strip->GetHeightForWidth(width)
                           : 0;
    layout.AddChild(views().webui_tab_strip,
                    gfx::Rect(params.visual_client_area.origin(),
                              gfx::Size(width, height)));
    params.Inset(gfx::Insets::TLBR(height, 0, 0, 0));
  }

  // Lay out horizontal tab strip region if present.
  if (IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().browser_view)) {
    gfx::Rect tabstrip_bounds;
    if (tab_strip_type == TabStripType::kHorizontal) {
      // Inset the leading edge of the tabstrip by the size of the swoop of the
      // first tab; this is especially important for Mac, where the negative
      // space of the caption button margins and the edge of the tabstrip should
      // overlap. The trailing edge receives the usual treatment, as it is the
      // new tab button and not a tab.
      tabstrip_bounds = GetBoundsWithExclusion(
          params, views().horizontal_tab_strip_region_view,
          TabStyle::Get()->GetBottomCornerRadius());
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));
      needs_exclusion = false;
    }
    layout.AddChild(views().horizontal_tab_strip_region_view, tabstrip_bounds,
                    tab_strip_type == TabStripType::kHorizontal);
  }

  // Lay out vertical tab strip if visible.
  int collapsed_vertical_tab_strip_adjustment = 0;
  if (IsParentedTo(views().vertical_tab_strip_region_view,
                   views().browser_view)) {
    gfx::Rect vertical_tab_strip_bounds;
    if (tab_strip_type == TabStripType::kVertical) {
      int vertical_tab_strip_relative_top = 0;
      if (horizontal_layout.vertical_tab_strip_collapsed_state ==
          VerticalTabStripCollapsedState::kCollapsed) {
        // Collapsed tabstrip sits underneath caption buttons when present.
        vertical_tab_strip_relative_top =
            GetCollapsedVerticalTabStripRelativeTop(params);
        collapsed_vertical_tab_strip_adjustment =
            vertical_tab_strip_relative_top > 0
                ? horizontal_layout.vertical_tab_strip_width
                : 0;
      }
      vertical_tab_strip_bounds = gfx::Rect(
          params.visual_client_area.x(),
          params.visual_client_area.y() + vertical_tab_strip_relative_top,
          horizontal_layout.vertical_tab_strip_width,
          params.visual_client_area.height() - vertical_tab_strip_relative_top);
      // In vertical tabs mode, extra space is allocated next to the top element
      // to serve as a grab handle, on whatever side the caption buttons are.
      if (delegate().GetBrowserWindowState() != WindowState::kFullscreen) {
        IncreasePaddingToMinimum(params, kVerticalTabsGrabHandleSize);
      }
      params.InsetHorizontal(horizontal_layout.vertical_tab_strip_width,
                             /*leading=*/true);
    }
    layout.AddChild(views().vertical_tab_strip_region_view,
                    vertical_tab_strip_bounds,
                    tab_strip_type == TabStripType::kVertical);
  }

  // Position the vertical tabstrip top corner.
  if (IsParentedTo(views().vertical_tab_strip_top_corner,
                   views().browser_view)) {
    gfx::Rect corner_bounds;

    // The tabstrip lays out below the leading exclusion when collapsed.
    const bool has_leading_exclusion =
        !browser_params.leading_exclusion.IsEmpty();
    const bool below_caption_buttons =
        horizontal_layout.vertical_tab_strip_collapsed_state ==
            VerticalTabStripCollapsedState::kCollapsed &&
        has_leading_exclusion;

    // The top corner is drawn when the tabstrip goes all the way to the top.
    const bool top_corner_visible =
        tab_strip_type == TabStripType::kVertical && !below_caption_buttons;
    if (top_corner_visible) {
      auto preferred =
          views().vertical_tab_strip_top_corner->GetPreferredSize();

      // The animation only needs to be applied when the tabstrip is switching
      // to/from a mode without a corner.
      if (has_leading_exclusion) {
        const auto percent =
            views()
                .vertical_tab_strip_region_view->GetCollapseAnimationPercent();
        if (percent.has_value()) {
          preferred.set_width(
              base::ClampCeil(preferred.width() * percent.value()));
        }
      }
      corner_bounds = gfx::Rect(params.visual_client_area.origin(), preferred);
    }
    layout.AddChild(views().vertical_tab_strip_top_corner, corner_bounds,
                    top_corner_visible);
  }

  // Position the vertical tabstrip bottom corner.
  if (IsParentedTo(views().vertical_tab_strip_bottom_corner,
                   views().browser_view)) {
    gfx::Rect corner_bounds;
    if (tab_strip_type == TabStripType::kVertical) {
      const auto preferred =
          views().vertical_tab_strip_bottom_corner->GetPreferredSize();
      corner_bounds =
          gfx::Rect(params.visual_client_area.x(),
                    params.visual_client_area.bottom() - preferred.height(),
                    preferred.width(), preferred.height());
    }
    layout.AddChild(views().vertical_tab_strip_bottom_corner, corner_bounds,
                    tab_strip_type == TabStripType::kVertical);
  }

  // TODO(crbug.com/469425263): Ensure correct layout calculations for the
  // Project Panel Container.
  if (IsParentedToAndVisible(views().projects_panel_container,
                             views().browser_view)) {
    const int target_width =
        views().projects_panel_container->GetPreferredSize().width();
    const double reveal_amount =
        views().projects_panel_container->GetResizeAnimationValue();
    const int visible_width = base::ClampFloor(target_width * reveal_amount);

    gfx::Rect projects_panel_bounds =
        gfx::Rect(browser_params.visual_client_area.x(),
                  browser_params.visual_client_area.y(), visible_width,
                  browser_params.visual_client_area.height());
    layout.AddChild(views().projects_panel_container, projects_panel_bounds);
  }

  // When the tabstrip isn't at the top or in constrained widths, the top
  // container is laid out before all side panels.
  if (horizontal_layout.force_top_container_to_top &&
      IsParentedTo(views().top_container, views().browser_view)) {
    auto& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());

    // Calculate the params for laying out the top container.
    auto top_container_params =
        params.InLocalCoordinates(params.visual_client_area);

    const gfx::Rect top_container_local_bounds = CalculateTopContainerLayout(
        top_container_layout, top_container_params, needs_exclusion);
    top_container_layout.bounds =
        GetTopContainerBoundsInParent(top_container_local_bounds, params);
    params.SetTop(top_container_layout.bounds.bottom());

    // Possibly bump the leading margin of the top container out to cover the
    // caption buttons, leaving all of the child views in the same absolute
    // position.
    if (collapsed_vertical_tab_strip_adjustment > 0) {
      top_container_layout.bounds.Outset(
          gfx::Outsets::TLBR(0, collapsed_vertical_tab_strip_adjustment, 0, 0));
      for (auto& [child, child_layout] : top_container_layout.children) {
        if (!child_layout.bounds.IsEmpty()) {
          child_layout.bounds.Offset(collapsed_vertical_tab_strip_adjustment,
                                     0);
        }
      }
    }
  }

  // Lay out the main area background.
  if (IsParentedTo(views().main_background_region, views().browser_view)) {
    layout.AddChild(views().main_background_region, params.visual_client_area,
                    horizontal_layout.has_toolbar_height_side_panel());
  }

  // Lay out toolbar-height side panel.
  bool toolbar_height_side_panel_leading = false;
  const double toolbar_height_side_panel_reveal_amount =
      horizontal_layout.has_toolbar_height_side_panel()
          ? views().toolbar_height_side_panel->GetAnimationValue()
          : 0.0;
  if (horizontal_layout.has_toolbar_height_side_panel()) {
    const SidePanel* const toolbar_height_side_panel =
        views().toolbar_height_side_panel;
    toolbar_height_side_panel_leading =
        toolbar_height_side_panel->IsRightAligned() == base::i18n::IsRTL();

    // Not all of the width may be visible on the screen.
    const int target_width = horizontal_layout.toolbar_height_side_panel_width;
    const int visible_width = base::ClampFloor(
        target_width * toolbar_height_side_panel_reveal_amount);

    // Add `container_inset_padding` to the top of the toolbar height side panel
    // to separate it from the horizontal tab strip. SidePanel draws the top on
    // top of the top content separator and some units of the toolbar by
    // default, which is not needed for the toolbar height side panel.
    const int top = params.visual_client_area.y() +
                    (tab_strip_type == TabStripType::kVertical
                         ? 0
                         : horizontal_layout.side_panel_padding);
    gfx::Rect toolbar_height_bounds(
        toolbar_height_side_panel_leading
            ? params.visual_client_area.x() - (target_width - visible_width)
            : params.visual_client_area.right() - visible_width,
        top, target_width, params.visual_client_area.bottom() - top);
    layout.AddChild(views().toolbar_height_side_panel, toolbar_height_bounds);

    // Lay out the animating contents.
    if (IsParentedToAndVisible(views().side_panel_animation_content,
                               views().browser_view)) {
      gfx::Rect toolbar_height_side_panel_final_bounds(
          toolbar_height_side_panel_leading
              ? params.visual_client_area.x()
              : params.visual_client_area.right() - target_width,
          toolbar_height_bounds.y(), target_width,
          toolbar_height_bounds.height());
      gfx::Rect side_panel_animation_content_bounds =
          views().toolbar_height_side_panel->GetContentAnimationBounds(
              toolbar_height_side_panel_final_bounds);

      layout.AddChild(views().side_panel_animation_content,
                      side_panel_animation_content_bounds);
    }

    params.InsetHorizontal(visible_width, toolbar_height_side_panel_leading);
  }

  const bool show_shadow_overlay = ShadowOverlayVisible();
  if (show_shadow_overlay) {
    // As the toolbar height side panel animates in, the main panel shrinks and
    // moves over to accommodate the panel.
    const int scaled_main_area_padding =
        base::ClampRound(toolbar_height_side_panel_reveal_amount *
                         horizontal_layout.side_panel_padding);
    params.Inset(gfx::Insets::TLBR(
        tab_strip_type == TabStripType::kVertical ? 0
                                                  : scaled_main_area_padding,
        toolbar_height_side_panel_leading ? 0 : scaled_main_area_padding,
        scaled_main_area_padding,
        toolbar_height_side_panel_leading ? scaled_main_area_padding : 0));
  }

  // Lay out the shadow overlay.
  layout.AddChild(views().main_shadow_overlay, params.visual_client_area,
                  show_shadow_overlay);

  // Lay out top container. The top container is laid out after the
  // toolbar-height side panel with a horizontal tabstrip if the browser is wide
  // enough.
  if (!horizontal_layout.force_top_container_to_top &&
      IsParentedTo(views().top_container, views().browser_view)) {
    auto& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());
    const gfx::Rect top_container_local_bounds = CalculateTopContainerLayout(
        top_container_layout,
        params.InLocalCoordinates(params.visual_client_area), needs_exclusion);
    top_container_layout.bounds =
        GetTopContainerBoundsInParent(top_container_local_bounds, params);
    params.SetTop(top_container_layout.bounds.bottom());
  }

  // Lay out infobar container.
  if (IsParentedTo(views().infobar_container, views().browser_view)) {
    gfx::Rect infobar_bounds;
    const bool infobar_visible = delegate().IsInfobarVisible();
    if (infobar_visible) {
      // Infobars slide down with top container reveal, but not when they're in
      // the toolbar-height side panel shadow box. This is because they only
      // slide down when they are visually adjacent to the toolbar/bookmarks
      // bar.
      const int additional_offset =
          show_shadow_overlay ? 0 : delegate().GetExtraInfobarOffset();
      infobar_bounds =
          gfx::Rect(params.visual_client_area.x(),
                    params.visual_client_area.y() + additional_offset,
                    params.visual_client_area.width(),
                    // This returns zero for empty infobar.
                    views().infobar_container->GetPreferredSize().height());
      params.SetTop(infobar_bounds.bottom());
    }
    layout.AddChild(views().infobar_container, infobar_bounds, infobar_visible);
  }

  // Lay out contents-height side panel.
  bool show_leading_separator = false;
  bool show_trailing_separator = false;
  bool contents_height_side_panel_leading = false;

  // The contents-height side panel is adjusted for the presence of a top
  // container separator in the browser view.
  const auto* top_separator_layout =
      layout.GetLayoutFor(views().top_container_separator);
  const int contents_height_side_panel_top =
      top_separator_layout && top_separator_layout->visibility.value()
          ? params.visual_client_area.y() - views::Separator::kThickness
          : params.visual_client_area.y();

  if (IsParentedTo(views().contents_height_side_panel, views().browser_view)) {
    const SidePanel* const contents_height_side_panel =
        views().contents_height_side_panel;
    const bool is_right_aligned = contents_height_side_panel->IsRightAligned();
    contents_height_side_panel_leading =
        is_right_aligned == base::i18n::IsRTL();
    if (horizontal_layout.has_content_height_side_panel()) {
      show_leading_separator = contents_height_side_panel_leading;
      show_trailing_separator = !contents_height_side_panel_leading;
    }

    const int target_width = horizontal_layout.content_height_side_panel_width;
    const int visible_width = base::ClampFloor(
        target_width * contents_height_side_panel->GetAnimationValue());

    // Side panel slides in from the edge of the main container.
    const gfx::Rect contents_height_side_panel_bounds(
        contents_height_side_panel_leading
            ? params.visual_client_area.x() - (target_width - visible_width)
            : params.visual_client_area.right() - visible_width,
        contents_height_side_panel_top, target_width,
        params.visual_client_area.bottom() - contents_height_side_panel_top);
    layout.AddChild(views().contents_height_side_panel,
                    contents_height_side_panel_bounds);
    params.InsetHorizontal(visible_width, contents_height_side_panel_leading);
  }

  // Show separators in multi-contents view. Note that the multi-contents
  // view is inside the main container so doesn't need to be laid out.
  views().multi_contents_view->SetShouldShowLeadingSeparator(
      show_leading_separator);
  views().multi_contents_view->SetShouldShowTrailingSeparator(
      show_trailing_separator);

  // Lay out contents container. The contents container contains the multi-
  // contents view when multi-contents are enabled. The checks here are to
  // force the logic to be updated when multi-contents is fully rolled-out.
  CHECK(
      IsParentedToAndVisible(views().contents_container, views().browser_view));
  CHECK(views().contents_container->Contains(views().multi_contents_view));

  // Because side panels have minimum width, in a small browser, it is possible
  // for the combination of minimum-sized contents pane and minimum-sized side
  // panel may exceed the width of the window. In this case, the contents pane
  // slides under the side panel.
  int content_left = params.visual_client_area.x();
  int content_right = params.visual_client_area.right();
  if (const int deficit = horizontal_layout.min_content_width -
                          params.visual_client_area.width();
      deficit > 0) {
    // Expand the contents by the deficit on the side with the side panel.
    params.InsetHorizontal(-deficit, contents_height_side_panel_leading);
    // However, do not let this go past the edge of the allowed area.
    content_left =
        std::max(content_left, browser_params.visual_client_area.x());
    content_right =
        std::min(content_right, browser_params.visual_client_area.right());
  }
  layout.AddChild(views().contents_container,
                  gfx::Rect(content_left, params.visual_client_area.y(),
                            content_right - content_left,
                            params.visual_client_area.height()));

  // Make final visual adjustments required for child views to paint.
  if (tab_strip_type == TabStripType::kVertical) {
    // Need to know the toolbar height relative to the tabstrip, so that
    // vertical tabstrip elements can align with the toolbar.
    const auto toolbar_bounds =
        layout.GetBoundsFor(views().toolbar, views().browser_view);
    const auto tabstrip_bounds = layout.GetBoundsFor(
        views().vertical_tab_strip_region_view, views().browser_view);
    CHECK(tabstrip_bounds);

    // Calculate the toolbar height adjacent to the tabstrip. This will be zero
    // if the toolbar is in e.g. an immersive mode overlay, or is not aligned
    // with the tabstrip (which can happen in collapsed mode with leading
    // caption buttons).
    const int toolbar_height =
        toolbar_bounds
            ? std::max(0, toolbar_bounds->bottom() - tabstrip_bounds->y())
            : 0;
    views().vertical_tab_strip_region_view->SetToolbarHeightForLayout(
        toolbar_height);

    // If the toolbar is not in the browser, then the exclusion isn't either.
    const int exclusion_width =
        toolbar_bounds
            ? std::max(0, base::ClampCeil(browser_params.leading_exclusion
                                              .ContentWithPadding()
                                              .width()) -
                              tabstrip_bounds->x())
            : 0;
    views().vertical_tab_strip_region_view->SetExclusionWidthForLayout(
        exclusion_width);
  }

  return layout;
}

gfx::Rect BrowserViewTabbedLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    BrowserLayoutParams params,
    bool needs_exclusion) const {
  const int original_top = params.visual_client_area.y();

  const TabStripType tab_strip_type = GetTabStripType();

  // If the WebUI tabstrip is in the top container (which can happen in
  // immersive mode), ensure it is laid out here.
  if (IsParentedTo(views().webui_tab_strip, views().top_container)) {
    const int width = params.visual_client_area.width();
    const int height = tab_strip_type == TabStripType::kWebUi &&
                               views().webui_tab_strip->GetVisible()
                           ? views().webui_tab_strip->GetHeightForWidth(width)
                           : 0;
    layout.AddChild(views().webui_tab_strip,
                    gfx::Rect(params.visual_client_area.origin(),
                              gfx::Size(width, height)));
    params.Inset(gfx::Insets::TLBR(height, 0, 0, 0));
  }

  // If the tabstrip is in the top container (which can happen in immersive
  // mode), ensure it is laid out here.
  if (IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().top_container)) {
    gfx::Rect tabstrip_bounds;
    if (tab_strip_type == TabStripType::kHorizontal) {
      // When there is an exclusion, inset the leading edge of the tabstrip by
      // the size of the swoop of the first tab; this is especially important
      // for Mac, where the negative space of the caption button margins and the
      // edge of the tabstrip should overlap. The trailing edge receives the
      // usual treatment, as it is the new tab button and not a tab.
      tabstrip_bounds = GetBoundsWithExclusion(
          params, views().horizontal_tab_strip_region_view,
          TabStyle::Get()->GetBottomCornerRadius());
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));
      needs_exclusion = false;
    }
    layout.AddChild(views().horizontal_tab_strip_region_view, tabstrip_bounds,
                    tab_strip_type == TabStripType::kHorizontal);
  }

  // Lay out toolbar. If tabstrip is completely absent (or vertical), this can
  // go in the top exclusion area.
  const bool toolbar_visible = delegate().IsToolbarVisible();
  if (IsParentedTo(views().toolbar, views().top_container)) {
    gfx::Rect toolbar_bounds;
    if (toolbar_visible) {
      toolbar_bounds =
          needs_exclusion
              ? GetBoundsWithExclusion(params, views().toolbar)
              : gfx::Rect(params.visual_client_area.x(),
                          params.visual_client_area.y(),
                          params.visual_client_area.width(),
                          views().toolbar->GetPreferredSize().height());
      params.SetTop(toolbar_bounds.bottom());
      needs_exclusion = false;
    }
    layout.AddChild(views().toolbar, toolbar_bounds, toolbar_visible);
  }

  // Lay out the bookmarks bar if one is present.
  const bool bookmarks_visible = delegate().IsBookmarkBarVisible();
  if (IsParentedTo(views().bookmark_bar, views().top_container)) {
    const gfx::Rect bookmarks_bounds(
        params.visual_client_area.x(), params.visual_client_area.y(),
        params.visual_client_area.width(),
        bookmarks_visible ? views().bookmark_bar->GetPreferredSize().height()
                          : 0);
    layout.AddChild(views().bookmark_bar, bookmarks_bounds, bookmarks_visible);
    params.SetTop(bookmarks_bounds.bottom());
  }

  // There are multiple different ways the top separator can render.
  const TopSeparatorType top_separator_type = GetTopSeparatorType();

  // Lay out the loading bar when present.
  if (IsParentedTo(views().loading_bar, views().top_container)) {
    gfx::Rect loading_bar_bounds;
    if (top_separator_type == TopSeparatorType::kLoadingBar) {
      loading_bar_bounds =
          gfx::Rect(params.visual_client_area.x(),
                    params.visual_client_area.y() - kLoadingBarOffset,
                    params.visual_client_area.width(), kLoadingBarHeight);
      params.SetTop(loading_bar_bounds.bottom());
    }
    layout.AddChild(views().loading_bar, loading_bar_bounds,
                    top_separator_type == TopSeparatorType::kLoadingBar);
  }

  // Maybe show the separator in the multi-contents view.
  views().multi_contents_view->SetShouldShowTopSeparator(
      top_separator_type == TopSeparatorType::kMultiContents);

  // Maybe show the separator in the top container.
  if (IsParentedTo(views().top_container_separator, views().top_container)) {
    gfx::Rect separator_bounds;
    if (top_separator_type == TopSeparatorType::kTopContainer) {
      separator_bounds = gfx::Rect(
          params.visual_client_area.x(), params.visual_client_area.y(),
          params.visual_client_area.width(),
          views().top_container_separator->GetPreferredSize().height());
      params.SetTop(separator_bounds.bottom());
    }
    layout.AddChild(views().top_container_separator, separator_bounds,
                    top_separator_type == TopSeparatorType::kTopContainer);
  }

  return gfx::Rect(params.visual_client_area.x(), original_top,
                   params.visual_client_area.width(),
                   params.visual_client_area.y() - original_top);
}

void BrowserViewTabbedLayoutImpl::ConfigureTopContainerBackground(
    const BrowserLayoutParams& params,
    CustomCornersBackground* background) {
  // Fall back to default implementation when vertical tabstrip not present.
  if (!delegate().ShouldDrawVerticalTabStrip()) {
    BrowserViewLayoutImpl::ConfigureTopContainerBackground(params, background);
    return;
  }

  // The top container always draws an opaque background when in vertical
  // tabstrip mode.
  background->SetVisible(true);
  background->SetPrimaryColor(CustomCornersBackground::TopContainerTheme());

  // Rounded corners are drawn when not maximized or fullscreen.
  CustomCornersBackground::Corners corners;
  if (delegate().GetBrowserWindowState() == WindowState::kNormal) {
    corners.upper_trailing = background->GetWindowCorner(/*upper=*/true);
    const bool vertical_tab_strip_reaches_top =
        GetVerticalTabStripCollapsedState() !=
            VerticalTabStripCollapsedState::kCollapsed ||
        params.leading_exclusion.IsEmpty();
    if (!vertical_tab_strip_reaches_top) {
      corners.upper_leading = background->GetWindowCorner(/*upper=*/true);
    }
  }
  background->SetCorners(corners);
}

void BrowserViewTabbedLayoutImpl::DoPostLayoutVisualAdjustments(
    const BrowserLayoutParams& params) {
  const auto tab_strip_type = GetTabStripType();
  const auto window_state = delegate().GetBrowserWindowState();
  bool vertical_tab_strip_reaches_top = false;

  // Set vertical tabstrip corners.
  if (tab_strip_type == TabStripType::kVertical) {
    // Vertical tabstrip goes all the way to the top of the window if it is not
    // collapsed or there are no caption buttons on the leading edge.
    vertical_tab_strip_reaches_top =
        GetVerticalTabStripCollapsedState() !=
            VerticalTabStripCollapsedState::kCollapsed ||
        params.leading_exclusion.IsEmpty();
    auto* const vertical_tabs_background =
        static_cast<CustomCornersBackground*>(
            views().vertical_tab_strip_region_view->background());
    CustomCornersBackground::Corners vertical_tabs_corners;
    // Ensure that corners of the window remain rounded.
    if (window_state == WindowState::kNormal) {
      if (vertical_tab_strip_reaches_top) {
        vertical_tabs_corners.upper_leading =
            vertical_tabs_background->GetWindowCorner(/*upper=*/true);
      }
      vertical_tabs_corners.lower_leading =
          vertical_tabs_background->GetWindowCorner(/*upper=*/false);
    }
    // When the vertical tabs are below the toolbar but next to the bookmarks
    // bar, draw a curved corner.
    if (!vertical_tab_strip_reaches_top &&
        window_state != WindowState::kFullscreen) {
      const auto* const toolbar_height_side_panel =
          views().toolbar_height_side_panel.get();
      const bool has_leading_side_panel =
          toolbar_height_side_panel &&
          toolbar_height_side_panel->GetVisible() &&
          toolbar_height_side_panel->IsRightAligned() == base::i18n::IsRTL();
      if (delegate().IsBookmarkBarVisible() || has_leading_side_panel) {
        vertical_tabs_corners.upper_trailing.type =
            CustomCornersBackground::CornerType::kRoundedWithBackground;
      }
    }
    vertical_tabs_background->SetCorners(vertical_tabs_corners);
  }

  // Set toolbar corners.
  auto* const toolbar_background =
      static_cast<CustomCornersBackground*>(views().toolbar->background());
  CustomCornersBackground::Corners toolbar_corners;
  switch (tab_strip_type) {
    case TabStripType::kHorizontal: {
      // Trailing curve is always shown for normal horizontal tabstrip.
      toolbar_corners.upper_trailing.type =
          CustomCornersBackground::CornerType::kRoundedWithBackground;

      // If there is anything on the leading side or the first tab is not
      // selected, then the corner radius is shown, otherwise we hide the
      // corner radius.
      if (!delegate().IsActiveTabAtLeadingWindowEdge()) {
        toolbar_corners.upper_leading.type =
            CustomCornersBackground::CornerType::kRoundedWithBackground;
      }
      break;
    }
    case TabStripType::kVertical: {
      if (window_state != WindowState::kFullscreen) {
        // Curve trailing corner when it goes all the way to the edge of the
        // browser.
        if (params.trailing_exclusion.IsEmpty()) {
          toolbar_corners.upper_trailing =
              toolbar_background->GetWindowCorner(/*upper=*/true);
        }
      }
      break;
    }
    case TabStripType::kWebUi:
      // In WebUI tabstrip mode, there's a titlebar at the top of the window
      // directly above the toolbar, so no corners are needed.
      break;
    default:
      // Ideally this should not be reached.
      break;
  }
  toolbar_background->SetCorners(toolbar_corners);

  if (views().main_background_region &&
      views().main_background_region->GetVisible()) {
    auto* const background = static_cast<CustomCornersBackground*>(
        views().main_background_region->background());
    CustomCornersBackground::Corners main_background_corners;

    // Frame-colored corners are shown at the top in horizontal tabstrip mode.
    // This doesn't apply in fullscreen, as the tabstrip is not in the window.
    if (tab_strip_type == TabStripType::kHorizontal &&
        window_state != WindowState::kFullscreen) {
      // If (due to narrow width) the top container is not laid out in the main
      // area, it also doesn't get rounded corners.
      if (views().main_background_region->y() <= views().top_container->y()) {
        if (!delegate().IsActiveTabAtLeadingWindowEdge()) {
          main_background_corners.upper_leading.type =
              CustomCornersBackground::CornerType::kRoundedWithBackground;
        }
        main_background_corners.upper_trailing.type =
            CustomCornersBackground::CornerType::kRoundedWithBackground;
      }
    }

    // Need to ensure the bottom of the window is properly rounded for normal
    // windows.
    if (window_state == WindowState::kNormal) {
      if (tab_strip_type != TabStripType::kVertical) {
        main_background_corners.lower_leading =
            background->GetWindowCorner(/*upper=*/false);
      }
      main_background_corners.lower_trailing =
          background->GetWindowCorner(/*upper=*/false);
    }

    background->SetCorners(main_background_corners);
  }
}
