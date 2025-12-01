// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// Loading bar is thicker than a separator, but instead of moving the bottom
// of the top container down, it starts above where the separator would go.
static constexpr int kLoadingBarHeight = 3;
static constexpr int kLoadingBarOffset =
    kLoadingBarHeight - views::Separator::kThickness;

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
      delegate().GetImmersiveModeController()->IsEnabled();
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

  // If there is no multi-contents view, there's nowhere else to put the
  // separator, so it goes in the top container.
  if (!views().multi_contents_view) {
    return TopSeparatorType::kTopContainer;
  }

  // If the infobar is visible, the separator has to go in the top container.
  if (IsInfobarVisible()) {
    return TopSeparatorType::kTopContainer;
  }

  // The separator should go in the multi contents view instead.
  return TopSeparatorType::kMultiContents;
}

std::pair<gfx::Size, gfx::Size>
BrowserViewTabbedLayoutImpl::GetMinimumTabStripSize() const {
  switch (GetTabStripType()) {
    case TabStripType::kHorizontal:
      return std::make_pair(gfx::Size(),
                            views().tab_strip_region_view->GetMinimumSize());
    case TabStripType::kVertical: {
      auto result = views().vertical_tab_strip_container->GetMinimumSize();
      result.set_width(std::max(result.width(), kMinVerticalTabStripWidth));
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

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumMainAreaSize() const {
  const gfx::Size toolbar_size = views().toolbar->GetMinimumSize();
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

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumSize(
    const views::View* host) const {
  // This is a simplified version of the same method in
  // `BrowserViewLayoutImplOld` that assumes a standard browser.
  const auto [vertical_tabstrip_size, horizontal_tabstrip_size] =
      GetMinimumTabStripSize();
  const gfx::Size toolbar_height_side_panel_size =
      views().toolbar_height_side_panel &&
              views().toolbar_height_side_panel->GetVisible()
          ? views().toolbar_height_side_panel->GetMinimumSize()
          : gfx::Size();
  const gfx::Size main_area_size = GetMinimumMainAreaSize();

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
        GetLayoutConstant(LayoutConstant::TOOLBAR_HEIGHT_SIDE_PANEL_INSET);
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
  if (IsParentedTo(views().tab_strip_region_view, views().browser_view)) {
    gfx::Rect tabstrip_bounds;
    if (tab_strip_type == TabStripType::kHorizontal) {
      // Inset the leading edge of the tabstrip by the size of the swoop of the
      // first tab; this is especially important for Mac, where the negative
      // space of the caption button margins and the edge of the tabstrip should
      // overlap. The trailing edge receives the usual treatment, as it is the
      // new tab button and not a tab.
      tabstrip_bounds =
          GetBoundsWithExclusion(params, views().tab_strip_region_view,
                                 TabStyle::Get()->GetBottomCornerRadius());
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP));
      needs_exclusion = false;
    }
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds,
                    tab_strip_type == TabStripType::kHorizontal);
  }

  // Lay out vertical tab strip if visible.
  if (IsParentedTo(views().vertical_tab_strip_container,
                   views().browser_view)) {
    gfx::Rect vertical_tab_strip_bounds;
    if (tab_strip_type == TabStripType::kVertical) {
      const int vertical_tab_strip_width = std::max(
          kMinVerticalTabStripWidth,
          views().vertical_tab_strip_container->GetPreferredSize().width());
      vertical_tab_strip_bounds = gfx::Rect(
          params.visual_client_area.x(), params.visual_client_area.y(),
          vertical_tab_strip_width, params.visual_client_area.height());
      params.Inset(gfx::Insets::TLBR(0, vertical_tab_strip_width, 0, 0));
    }
    layout.AddChild(views().vertical_tab_strip_container,
                    vertical_tab_strip_bounds,
                    tab_strip_type == TabStripType::kVertical);
  }

  // Figure out whether the toolbar-height side panel should show and by how
  // much.
  const bool has_toolbar_height_side_panel = IsParentedToAndVisible(
      views().toolbar_height_side_panel, views().browser_view);

  // Lay out the main area background.
  if (IsParentedTo(views().main_background_region, views().browser_view)) {
    layout.AddChild(views().main_background_region, params.visual_client_area,
                    has_toolbar_height_side_panel);
  }

  // The insets for main region and its containing views when the
  // toolbar_height_side_panel is visible.
  const int container_inset_padding =
      GetLayoutConstant(LayoutConstant::TOOLBAR_HEIGHT_SIDE_PANEL_INSET);

  // Lay out toolbar-height side panel.
  bool toolbar_height_side_panel_leading = false;
  const double toolbar_height_side_panel_reveal_amount =
      has_toolbar_height_side_panel
          ? views().toolbar_height_side_panel->GetAnimationValue()
          : 0.0;
  if (IsParentedToAndVisible(views().toolbar_height_side_panel,
                             views().browser_view)) {
    const SidePanel* const toolbar_height_side_panel =
        views().toolbar_height_side_panel;
    toolbar_height_side_panel_leading =
        toolbar_height_side_panel->IsRightAligned() == base::i18n::IsRTL();

    // Side panel needs to fit next to the other stuff in the browser, but it
    // always gets at least its minimum width.
    int target_width = toolbar_height_side_panel->GetPreferredSize().width();
    target_width = std::min(
        target_width,
        params.visual_client_area.width() -
            (GetMinimumMainAreaSize().width() + container_inset_padding));
    target_width = std::max(
        target_width, toolbar_height_side_panel->GetMinimumSize().width());

    // Not all of the width may be visible on the screen.
    const int visible_width = base::ClampFloor(
        target_width * toolbar_height_side_panel_reveal_amount);

    // Add `container_inset_padding` to the top of the toolbar height side panel
    // to separate it from the tab strip. SidePanel draws the top on top of the
    // top content separator and some units of the toolbar by default, which is
    // not needed for the toolbar height side panel.
    const int top = params.visual_client_area.y() + container_inset_padding;
    gfx::Rect toolbar_height_bounds(
        toolbar_height_side_panel_leading
            ? params.visual_client_area.x() - (target_width - visible_width)
            : params.visual_client_area.right() - visible_width,
        top, target_width, params.visual_client_area.bottom() - top);
    layout.AddChild(views().toolbar_height_side_panel, toolbar_height_bounds);

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

  // Lay out the shadow overlay.
  const bool show_shadow_overlay = ShadowOverlayVisible();
  if (show_shadow_overlay) {
    // As the toolbar height side panel animates in, the main panel shrinks and
    // moves over to accommodate the panel.
    const int scaled_main_area_padding = base::ClampRound(
        toolbar_height_side_panel_reveal_amount * container_inset_padding);
    params.Inset(gfx::Insets::TLBR(
        scaled_main_area_padding,
        toolbar_height_side_panel_leading ? 0 : scaled_main_area_padding,
        scaled_main_area_padding,
        toolbar_height_side_panel_leading ? scaled_main_area_padding : 0));
  }

  // Lay out the remainder of the main container.
  layout.AddChild(views().main_shadow_overlay, params.visual_client_area,
                  show_shadow_overlay);

  // Lay out top container.
  if (IsParentedTo(views().top_container, views().browser_view)) {
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
  bool show_left_separator = false;
  bool show_right_separator = false;
  bool contents_height_side_panel_leading = false;
  int min_contents_width = kContentsContainerMinimumWidth;

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
    int contents_height_side_panel_width = 0;
    int contents_height_side_panel_visible_width = 0;
    const bool is_right_aligned = contents_height_side_panel->IsRightAligned();
    contents_height_side_panel_leading =
        is_right_aligned == base::i18n::IsRTL();
    if (contents_height_side_panel->GetVisible()) {
      // Side panel implies a separator, which means we have to give a little
      // more room for the contents.
      min_contents_width += views::Separator::kThickness;
      show_left_separator = !is_right_aligned;
      show_right_separator = is_right_aligned;

      // Maximum width is the lesser of preferred width and the largest width
      // that doesn't shrink the contents pane past its own minimum size.
      const int min_width =
          contents_height_side_panel->GetMinimumSize().width();
      const int preferred_width =
          contents_height_side_panel->GetPreferredSize().width();
      int max_width =
          std::min(preferred_width,
                   params.visual_client_area.width() - min_contents_width);
      if (contents_height_side_panel->ShouldRestrictMaxWidth()) {
        max_width =
            std::min(max_width, params.visual_client_area.width() * 2 / 3);
      }

      // Side panel always gets at least its minimum width.
      contents_height_side_panel_width = std::max(min_width, max_width);
      contents_height_side_panel_visible_width =
          base::ClampFloor(contents_height_side_panel_width *
                           contents_height_side_panel->GetAnimationValue());
    }

    // Side panel slides in from the edge of the main container.
    const gfx::Rect contents_height_side_panel_bounds(
        contents_height_side_panel_leading
            ? params.visual_client_area.x() -
                  (contents_height_side_panel_width -
                   contents_height_side_panel_visible_width)
            : params.visual_client_area.right() -
                  contents_height_side_panel_visible_width,
        contents_height_side_panel_top, contents_height_side_panel_width,
        params.visual_client_area.bottom() - contents_height_side_panel_top);
    layout.AddChild(views().contents_height_side_panel,
                    contents_height_side_panel_bounds);
    params.InsetHorizontal(contents_height_side_panel_visible_width,
                           contents_height_side_panel_leading);
  }

  // This will be used to position the separator corner.
  const int separator_edge = contents_height_side_panel_leading
                                 ? params.visual_client_area.x()
                                 : params.visual_client_area.right();

  // Maybe show separators in multi-contents view. If this happens, the
  // separators aren't shown in the main container. Note that the multi-contents
  // view is inside the main container so doesn't need to be laid out.
  if (views().multi_contents_view) {
    bool show_leading_separator = false;
    bool show_trailing_separator = false;
    if (show_left_separator || show_right_separator) {
      show_leading_separator = contents_height_side_panel_leading;
      show_trailing_separator = !contents_height_side_panel_leading;
    }
    views().multi_contents_view->SetShouldShowLeadingSeparator(
        show_leading_separator);
    views().multi_contents_view->SetShouldShowTrailingSeparator(
        show_trailing_separator);
    show_left_separator = false;
    show_right_separator = false;
  }

  // Lay out the left side panel separator.
  if (IsParentedTo(views().left_aligned_side_panel_separator,
                   views().browser_view)) {
    gfx::Rect separator_bounds;
    if (show_left_separator) {
      const int separator_width =
          views().left_aligned_side_panel_separator->GetPreferredSize().width();
      separator_bounds =
          gfx::Rect(contents_height_side_panel_leading
                        ? params.visual_client_area.x()
                        : params.visual_client_area.right() - separator_width,
                    params.visual_client_area.y(), separator_width,
                    params.visual_client_area.height());
      params.InsetHorizontal(separator_width,
                             contents_height_side_panel_leading);
    }
    layout.AddChild(views().left_aligned_side_panel_separator, separator_bounds,
                    show_left_separator);
  }

  // Lay out the right side panel separator.
  if (IsParentedTo(views().right_aligned_side_panel_separator,
                   views().browser_view)) {
    gfx::Rect separator_bounds;
    if (show_right_separator) {
      const int separator_width =
          views()
              .right_aligned_side_panel_separator->GetPreferredSize()
              .width();
      separator_bounds =
          gfx::Rect(contents_height_side_panel_leading
                        ? params.visual_client_area.x()
                        : params.visual_client_area.right() - separator_width,
                    params.visual_client_area.y(), separator_width,
                    params.visual_client_area.height());
      params.InsetHorizontal(separator_width,
                             contents_height_side_panel_leading);
    }
    layout.AddChild(views().right_aligned_side_panel_separator,
                    separator_bounds, show_right_separator);
  }

  // Lay out the corner separator.
  if (IsParentedTo(views().side_panel_rounded_corner, views().browser_view)) {
    const bool visible = show_left_separator || show_right_separator;
    gfx::Rect corner_bounds;
    if (visible) {
      const gfx::Size corner_size =
          views().side_panel_rounded_corner->GetPreferredSize();
      const gfx::Point corner_pos(contents_height_side_panel_leading
                                      ? separator_edge
                                      : separator_edge - corner_size.width(),
                                  contents_height_side_panel_top);
      corner_bounds = gfx::Rect(corner_pos, corner_size);
    }
    layout.AddChild(views().side_panel_rounded_corner, corner_bounds, visible);
  }

  // Lay out contents container. The contents container contains the multi-
  // contents view when multi-contents are enabled. The checks here are to
  // force the logic to be updated when multi-contents is fully rolled-out.
  CHECK(
      IsParentedToAndVisible(views().contents_container, views().browser_view));
  CHECK(views().multi_contents_view == nullptr ||
        views().contents_container->Contains(views().multi_contents_view));

  // Because side panels have minimum width, in a small browser, it is possible
  // for the combination of minimum-sized contents pane and minimum-sized side
  // panel may exceed the width of the window. In this case, the contents pane
  // slides under the side panel.
  int content_left = params.visual_client_area.x();
  int content_right = params.visual_client_area.right();
  if (const int deficit =
          min_contents_width - params.visual_client_area.width();
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
  if (IsParentedTo(views().tab_strip_region_view, views().top_container)) {
    gfx::Rect tabstrip_bounds;
    if (tab_strip_type == TabStripType::kHorizontal) {
      // When there is an exclusion, inset the leading edge of the tabstrip by
      // the size of the swoop of the first tab; this is especially important
      // for Mac, where the negative space of the caption button margins and the
      // edge of the tabstrip should overlap. The trailing edge receives the
      // usual treatment, as it is the new tab button and not a tab.
      tabstrip_bounds =
          GetBoundsWithExclusion(params, views().tab_strip_region_view,
                                 TabStyle::Get()->GetBottomCornerRadius());
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP));
      needs_exclusion = false;
    }
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds,
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
  if (views().multi_contents_view) {
    views().multi_contents_view->SetShouldShowTopSeparator(
        top_separator_type == TopSeparatorType::kMultiContents);
  }

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
