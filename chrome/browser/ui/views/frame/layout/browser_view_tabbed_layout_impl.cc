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
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/animations/side_panel_animations.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/custom_floating_corner.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "chrome/browser/ui/views/frame/main_background_region_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/shadow_frame_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// Minimum area next to caption buttons to use as a grab handle.
constexpr int kVerticalTabsGrabHandleSize = 40;

// Maximum portion of the window a "size-restricted" contents-height side panel
// can take up. This is not the only limit on side panel size.
constexpr float kMaxContentsHeightSidePanelFraction = 2.f / 3.f;

// How much the vertical tab strip's outline fades out when expanding-on-hover.
// This is to help it blend better with the drop shadow it acquires - especially
// in dark mode, where the outline is lighter than the tab strip but the shadow
// is darker.
//
// This is a percentage, with 0.0 meaning no change to the outline, and 1.0
// meaning the outline disappears completely.
constexpr double kVerticalTabStripOutlineFadeOnHover = 0.5;

// Increases the leading or trailing exclusion padding to `minimum`.
void IncreasePaddingToMinimum(BrowserLayoutParams& params, int minimum) {
  // Default to increasing the trailing exclusion padding. On ChromeOS, the
  // leading exclusion is sometimes used for a profile avatar while the trailing
  // exclusion is used for the caption buttons. This keeps the padding
  // consistent.
  if (params.trailing_exclusion.content.width() > 0.0) {
    params.trailing_exclusion.horizontal_padding =
        std::max(params.trailing_exclusion.horizontal_padding,
                 static_cast<float>(minimum));
  } else {
    params.leading_exclusion.horizontal_padding =
        std::max(params.leading_exclusion.horizontal_padding,
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

void InsetHorizontal(gfx::Rect& rect, int amount, bool leading) {
  rect.Inset(
      gfx::Insets::TLBR(0, leading ? amount : 0, 0, !leading ? amount : 0));
}

}  // namespace

// Allocate space across the vertical tabstrip, toolbar, and side panels,
// possibly modifying `params` to allocate grab handle space, and
// determining how much space to give to each of the left-size elements.
struct BrowserViewTabbedLayoutImpl::HorizontalLayout {
  VerticalTabStripCollapsedState vertical_tab_strip_collapsed_state =
      VerticalTabStripCollapsedState::kExpanded;
  int vertical_tab_strip_width = 0;
  int side_panel_width = 0;
  int min_content_width = 0;

  // The padding placed around a number of UI elements when the side panel is
  // present.
  int side_panel_padding = 0;

  // In some cases, even when there is a side panel, the top container
  // (containing the toolbar, etc.) are laid out at the top of the screen,
  // above the side panels - this is usually due to other layout
  // constraints.
  bool force_top_container_to_top = false;

  // True if the split view is present.
  bool is_split_view = false;

  bool has_side_panel() const { return side_panel_width > 0; }
};

// Describes how to render the top of the vertical tab strip.
struct BrowserViewTabbedLayoutImpl::VerticalTabStripAnimation {
  // Is the vertical tab strip animating?
  BrowserAnimationMotion current_motion;
  // The y-value of the top of the tab strip.
  int top_offset = 0;
  // The relative size of the top corner.
  double top_corner = 0.0;
  // The relative size of the bottom corner.
  double bottom_corner = 0.0;
  // How much of the expand-on-hover is shown.
  double expand_on_hover = 0.0;
  // How much the tab strip is expanded, not on-hover.
  double tab_strip_width = 0.0;
};

// Stores information about visual separators throughout the browser.
struct BrowserViewTabbedLayoutImpl::SeparatorInfo {
  // True if the separator should be rendered in the top container.
  bool top_container_separator = false;
  // True if the separator should be rendered in the multi-contents view.
  bool multi_contents_separator = false;
  // True if the shadow box should be rendered.
  bool shadow_box = false;
  // Whether to move the elements down to account for a separator that's present
  // only some of the time.
  bool adjust_for_missing_separator = false;
  // Whether to pad the top of the side panel.
  bool side_panel_top_padding = false;
};

// Data computed during layout which is discarded afterwards.
// Members are presented in the order they're computed.
struct BrowserViewTabbedLayoutImpl::TransientLayoutData {
  explicit TransientLayoutData(const BrowserLayoutParams& browser_params)
      : revised_params(browser_params) {}
  ~TransientLayoutData() = default;

  // These parameters should be used by the layout function and may include
  // additional padding, etc. They are generated
  BrowserLayoutParams revised_params;
  WindowState window_state;
  TabStripType tab_strip_type;
  HorizontalLayout horizontal_layout;
  VerticalTabStripAnimation vertical_tab_strip_animation;
  SeparatorInfo separator_info;
};

BrowserViewTabbedLayoutImpl::BrowserViewTabbedLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayoutImpl(std::move(delegate), browser, std::move(views)) {}

BrowserViewTabbedLayoutImpl::~BrowserViewTabbedLayoutImpl() = default;

BrowserViewTabbedLayoutImpl::SeparatorInfo
BrowserViewTabbedLayoutImpl::CalculateSeparatorInfo() const {
  SeparatorInfo info;

  const bool has_side_panel = layout_data_->horizontal_layout.has_side_panel();
  const bool is_force_to_top =
      has_side_panel &&
      layout_data_->horizontal_layout.force_top_container_to_top;
  const bool has_infobar = delegate().IsInfobarVisible();
  const bool is_split_view = layout_data_->horizontal_layout.is_split_view;

  const bool would_have_separator = has_infobar || !is_split_view;
  switch (layout_data_->window_state) {
    case WindowState::kNormal:
    case WindowState::kMaximized: {
      // In content-height mode, split view outline serves as shadow box.
      info.shadow_box =
          has_side_panel && (has_infobar || !is_split_view || !is_force_to_top);
      const bool suppress_separator = info.shadow_box && is_force_to_top;
      info.multi_contents_separator = !suppress_separator && !has_infobar;
      info.top_container_separator = !suppress_separator && has_infobar;
      info.side_panel_top_padding = !is_force_to_top;
      info.adjust_for_missing_separator =
          suppress_separator && would_have_separator;
      break;
    }
    case WindowState::kFullscreen:
      // In fullscreen, split view outline serves as shadow box.
      info.shadow_box = has_side_panel && (has_infobar || !is_split_view);
      // Mac and ChromeOS display separators on their slide-in top containers.
      // However, if top container elements are not visible (for example,
      // because they are offscreen), the separator is not shown.
      info.top_container_separator =
          delegate().IsToolbarVisible() || delegate().IsBookmarkBarVisible();
      info.side_panel_top_padding = true;
      break;
    case WindowState::kFullscreenWithToolbar:
      // In fullscreen, split view outline serves as shadow box.
      info.shadow_box = has_side_panel && (has_infobar || !is_split_view);
      info.top_container_separator = !info.shadow_box && would_have_separator;
      info.adjust_for_missing_separator =
          info.shadow_box && would_have_separator;
      break;
  }

  return info;
}

// Inset the leading edge of the tabstrip by the size of the swoop of the
// first tab; this is especially important for Mac, where the negative
// space of the caption button margins and the edge of the tabstrip should
// overlap. This only applies if there are no other leading buttons; if
// there are, we want a consistent gap from the caption buttons. The
// trailing edge receives the usual treatment, as it is the new tab button
// and not a tab.
int BrowserViewTabbedLayoutImpl::GetHorizontalTabStripLeadingMargin(
    const BrowserLayoutParams& params) const {
  int leading_margin = TabStyle::Get()->GetBottomCornerRadius();
  if (const gfx::Insets* internal_padding =
          views().horizontal_tab_strip_region_view->GetProperty(
              views::kInternalPaddingKey)) {
    leading_margin =
        std::max(0.f, params.leading_exclusion.horizontal_padding -
                          static_cast<float>(internal_padding->left()));
  }
  return leading_margin;
}

bool BrowserViewTabbedLayoutImpl::AvoidCrackingForFractionalDisplay() const {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  // This is primarily an issue on Linux and Windows; add other platforms here
  // as needed.
  if (auto* const widget = views().browser_view->GetWidget()) {
    if (const auto display = widget->GetNearestDisplay()) {
      const float scale = display->device_scale_factor();
      return scale != std::floor(scale);
    }
  }
#endif
  return false;
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
      if (GetVerticalTabStripCollapsedState() ==
          VerticalTabStripCollapsedState::kCollapsed) {
        // With a collapsed tabstrip, the tabstrip sits below the leading
        // exclusion. Add this additional height so that it's accounted for in
        // minimum size computations.
        result.set_height(
            result.height() +
            base::ClampCeil(
                params.leading_exclusion.ContentWithPadding().height()));
        // Reserve enough width to uncollapse the tabstrip even if it's
        // collapsed, or else uncollapsing the tabstrip will break the browser.
        result.set_width(std::max(
            result.width(), VerticalTabStripRegionView::kUncollapsedMinWidth));
      } else {
        result.set_width(std::max(
            result.width(),
            base::ClampCeil(
                params.leading_exclusion.ContentWithPadding().width())));
      }
      return std::make_pair(result, gfx::Size());
    }
    case TabStripType::kNone:
      return std::make_pair(gfx::Size(), gfx::Size());
  }
}

BrowserViewTabbedLayoutImpl::HorizontalLayout
BrowserViewTabbedLayoutImpl::CalculateHorizontalLayout(
    BrowserLayoutParams& params) const {
  HorizontalLayout layout;
  // Start with some preliminary values.
  layout.force_top_container_to_top =
      layout_data_->tab_strip_type != TabStripType::kHorizontal;

  layout.is_split_view = delegate().IsActiveTabSplit();

  if (views().side_panel &&
      views().side_panel->GetCurrentEntryType() == SidePanelType::kContent) {
    layout.force_top_container_to_top = true;
  }

  layout.min_content_width = kContentsContainerMinimumWidth;

  // Get information about the vertical tabstrip, if present.
  const int toolbar_minimum_width = views().toolbar->GetMinimumSize().width();
  int min_vertical_tab_strip_width = 0;
  int preferred_vertical_tab_strip_width = 0;
  if (layout_data_->tab_strip_type == TabStripType::kVertical) {
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

      // Account for grab handle.
      IncreasePaddingToMinimum(params, GetMinimumGrabHandlePadding());

    } else {
      // Minimum size is bounded from below by size of leading exclusion area.
      if (layout.vertical_tab_strip_collapsed_state ==
          VerticalTabStripCollapsedState::kExpanded) {
        min_vertical_tab_strip_width = std::max(
            vertical_tab_strip->GetMinimumSize().width(),
            base::ClampCeil(
                params.leading_exclusion.ContentWithPadding().width()));
      } else {
        min_vertical_tab_strip_width =
            vertical_tab_strip->GetMinimumSize().width();
      }

      // Account for grab handle. This has to be done after the minimum size
      // calculation.
      IncreasePaddingToMinimum(params, GetMinimumGrabHandlePadding());

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
  }

  // Get information about the toolbar-height side panel, if present.
  int min_side_panel_width = 0;
  int preferred_side_panel_width = 0;
  if (const auto* const panel = views().side_panel.get();
      IsParentedToAndVisible(panel, views().browser_view)) {
    min_side_panel_width = panel->GetMinimumSize().width();
    preferred_side_panel_width = panel->GetPreferredSize().width();

    if (panel->GetCurrentEntryType() == SidePanelType::kContent &&
        panel->ShouldRestrictMaxWidth()) {
      preferred_side_panel_width =
          std::min(preferred_side_panel_width,
                   base::ClampFloor(params.visual_client_area.width() *
                                    kMaxContentsHeightSidePanelFraction));
    }
    preferred_side_panel_width =
        std::max(min_side_panel_width, preferred_side_panel_width);

    // Add additional padding.
    layout.side_panel_padding =
        GetLayoutConstant(LayoutConstant::kSidePanelInset);

    // See if the toolbar-height side panel can fit next to the toolbar. If not,
    // it is forced into content height.
    if (!layout.force_top_container_to_top) {
      const int remainder = params.visual_client_area.width() -
                            (toolbar_minimum_width + layout.side_panel_padding);
      layout.force_top_container_to_top = remainder < min_side_panel_width;

      // If still allowing toolbar height, clamp the side panel based on what
      // the toolbar actually supports.
      if (!layout.force_top_container_to_top) {
        preferred_side_panel_width =
            std::min(preferred_side_panel_width, remainder);
      }
    }
  }

  // If the top container is not forced to the top, it occupies the same
  // horizontal row as the side panel. In this case, ensure that the minimum
  // width of the content area includes the minimum width of the toolbar.
  if (IsParentedToAndVisible(views().side_panel.get(), views().browser_view) &&
      !layout.force_top_container_to_top) {
    layout.min_content_width =
        std::max(layout.min_content_width, toolbar_minimum_width);
  }

  // Start with the minimum values for each element.
  layout.vertical_tab_strip_width = min_vertical_tab_strip_width;
  layout.side_panel_width = min_side_panel_width;

  // Determine how much space is left to allocate.
  int remaining = params.visual_client_area.width() - layout.min_content_width -
                  layout.side_panel_padding - min_vertical_tab_strip_width -
                  min_side_panel_width;

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
        std::min(remaining, preferred_side_panel_width - min_side_panel_width);
    layout.side_panel_width += toolbar_height_flex_amount;
  }

  return layout;
}

BrowserViewTabbedLayoutImpl::VerticalTabStripAnimation
BrowserViewTabbedLayoutImpl::CalculateVerticalTabStripAnimation() {
  // Since the state of the side panel and bookmarks can change outside of
  // tabstrip animations, maybe update the top corner animation value for a
  // collapsed tabstrip. This must be done before layout is calculated.

  const auto* const side_panel = views().side_panel.get();
  const bool has_side_panel = side_panel && side_panel->GetVisible();
  const double open_amount =
      has_side_panel ? side_panel->GetAnimationValue() : 0;
  const double side_panel_corner_value =
      gfx::Tween::DoubleValueBetween(open_amount, 0.0, -1.0);
  const bool is_split_view = layout_data_->horizontal_layout.is_split_view;
  const bool has_infobar = delegate().IsInfobarVisible();
  std::optional<double> max_uncollapsed_top_corner;

  if (layout_data_->tab_strip_type == TabStripType::kVertical) {
    double top_corner_collapsed_state = 1.0;
    const bool would_have_separator = has_infobar || !is_split_view;
    switch (layout_data_->window_state) {
      case WindowState::kFullscreen:
        top_corner_collapsed_state = 1.0;
        break;
      case WindowState::kFullscreenWithToolbar:
        top_corner_collapsed_state =
            would_have_separator ? side_panel_corner_value : -1.0;
        max_uncollapsed_top_corner = top_corner_collapsed_state;
        break;
      case WindowState::kNormal:
      case WindowState::kMaximized:
        if (layout_data_->revised_params.leading_exclusion.ContentWithPadding()
                .height() > 0) {
          top_corner_collapsed_state =
              delegate().IsBookmarkBarVisible() || !would_have_separator
                  ? -1.0
                  : side_panel_corner_value;
        }
        break;
    }

    auto* const controller = BrowserAnimationController::From(browser());
    auto* const animations =
        controller->GetAnimationProvider<TabStripAnimations>();
    animations->UpdateDefaultValue(TabStripAnimations::kVerticalTabStrip,
                                   TabStripAnimations::kTopCorner,
                                   top_corner_collapsed_state);
  }

  // Now that the proper default value is set, calculate all of the corner
  // animation values.
  int leading_exclusion_height = GetCollapsedVerticalTabStripRelativeTop();
  VerticalTabStripAnimation animation;
  auto* const controller = BrowserAnimationController::From(browser());
  animation.current_motion =
      controller->GetCurrentMotion(TabStripAnimations::kVerticalTabStrip);
  animation.top_offset = base::ClampRound(
      leading_exclusion_height *
      *controller->GetCurrentValue(TabStripAnimations::kVerticalTabStrip,
                                   TabStripAnimations::kTabStripTop));
  animation.expand_on_hover =
      *controller->GetCurrentValue(TabStripAnimations::kVerticalTabStrip,
                                   TabStripAnimations::kTabStripHoverWidth);
  animation.tab_strip_width =
      *controller->GetCurrentValue(TabStripAnimations::kVerticalTabStrip,
                                   TabStripAnimations::kTabStripWidth);
  animation.top_corner = *controller->GetCurrentValue(
      TabStripAnimations::kVerticalTabStrip, TabStripAnimations::kTopCorner);
  if (animation.expand_on_hover == 0.0 && max_uncollapsed_top_corner) {
    animation.top_corner =
        std::min(animation.top_corner, *max_uncollapsed_top_corner);
  }
  animation.bottom_corner = *controller->GetCurrentValue(
      TabStripAnimations::kVerticalTabStrip, TabStripAnimations::kBottomCorner);

  return animation;
}

int BrowserViewTabbedLayoutImpl::GetMinimumGrabHandlePadding() const {
  if (base::FeatureList::IsEnabled(features::kVerticalTabsGrabHandleRemoval)) {
    if (features::kVerticalTabsGrabHandleRemovalAlways.Get() ||
        GetVerticalTabStripCollapsedState() ==
            VerticalTabStripCollapsedState::kExpanded) {
      return 0;
    }
  }

  return kVerticalTabsGrabHandleSize -
         GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN).right();
}

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumMainAreaSize(
    const BrowserLayoutParams& params) const {
  gfx::Size toolbar_size = views().toolbar->GetMinimumSize();
  const auto tab_strip_type = GetTabStripType();
  if (tab_strip_type == TabStripType::kVertical) {
    toolbar_size.Enlarge(GetExclusionWidth(params), 0);
  }
  const gfx::Size bookmark_bar_size =
      (views().bookmark_bar && views().bookmark_bar->GetVisible())
          ? views().bookmark_bar->GetMinimumSize()
          : gfx::Size();
  const gfx::Size infobar_container_size =
      views().infobar_container->GetMinimumSize();
  const gfx::Size contents_size = views().contents_container->GetMinimumSize();

  int width = std::max({toolbar_size.width(), bookmark_bar_size.width(),
                        infobar_container_size.width(),
                            kContentsContainerMinimumWidth});
  const int height = toolbar_size.height() + bookmark_bar_size.height() +
                     infobar_container_size.height() + contents_size.height();

  return gfx::Size(width, height);
}

BrowserViewTabbedLayoutImpl::TabStripType
BrowserViewTabbedLayoutImpl::GetTabStripType() const {
  if (delegate().ShouldDrawVerticalTabStrip()) {
#if BUILDFLAG(IS_MAC)
    // Do not lay out the vertical tabstrip in content-fullscreen on Mac. This
    // check cannot be done in BrowserView because the immersive mode controller
    // itself relies on BrowserView reporting which tab strip it *would* draw,
    // creating a circular dependency/race condition.
    if (fullscreen_utils::IsInContentFullscreen(browser())) {
      return TabStripType::kNone;
    }
#endif
    return TabStripType::kVertical;
  }
  return delegate().ShouldDrawTabStrip() ? TabStripType::kHorizontal
                                         : TabStripType::kNone;
}

BrowserViewTabbedLayoutImpl::VerticalTabStripCollapsedState
BrowserViewTabbedLayoutImpl::GetVerticalTabStripCollapsedState() const {
  if (!views().vertical_tab_strip_region_view) {
    return VerticalTabStripCollapsedState::kExpanded;
  }
  const auto motion =
      BrowserAnimationController::From(browser())->GetCurrentMotion(
          TabStripAnimations::kVerticalTabStrip);
  if (motion == TabStripAnimations::kCollapse) {
    return VerticalTabStripCollapsedState::kCollapsing;
  }
  if (motion == TabStripAnimations::kExpand) {
    return VerticalTabStripCollapsedState::kExpanding;
  }
  if (delegate().IsVerticalTabStripCollapsed()) {
    return VerticalTabStripCollapsedState::kCollapsed;
  }
  return VerticalTabStripCollapsedState::kExpanded;
}

int BrowserViewTabbedLayoutImpl::GetCollapsedVerticalTabStripRelativeTop()
    const {
  // When the top container isn't in the browser view, the exclusion won't apply
  // and the tabstrip goes all the way to the top.
  if (!IsParentedTo(views().top_container, views().browser_view)) {
    return 0;
  }

  const auto& params = layout_data_->revised_params;

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
    IncreasePaddingToMinimum(params, GetMinimumGrabHandlePadding());
  }
  params.InsetHorizontal(vertical_tabstrip_size.width(), /*leading=*/true);
  const gfx::Size side_panel_size =
      views().side_panel && views().side_panel->GetVisible()
          ? views().side_panel->GetMinimumSize()
          : gfx::Size();
  const gfx::Size main_area_size = GetMinimumMainAreaSize(params);

  int min_height = horizontal_tabstrip_size.height() +
                   std::max({side_panel_size.height(), main_area_size.height(),
                             vertical_tabstrip_size.height()});

  // This assumes a horizontal tabstrip. There is also a hard minimum on the
  // width of the browser defined by `kMainBrowserContentsMinimumWidth`.
  int min_width = vertical_tabstrip_size.width() +
                  std::max({horizontal_tabstrip_size.width(),
                            side_panel_size.width() + main_area_size.width(),
                            kMainBrowserContentsMinimumWidth});

  // Maybe adjust for additional padding when the side panel is visible.
  if (side_panel_size.width() > 0 && !delegate().IsActiveTabSplit()) {
    const auto padding = GetLayoutConstant(LayoutConstant::kSidePanelInset);
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

  BrowserLayoutParams params = layout_data_->revised_params;
  bool needs_exclusion = true;
  const bool adjust_for_cracking = AvoidCrackingForFractionalDisplay();
  const HorizontalLayout& horizontal_layout = layout_data_->horizontal_layout;

  // Lay out horizontal tab strip region if present.
  if (IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().browser_view)) {
    gfx::Rect tabstrip_bounds;
    if (layout_data_->tab_strip_type == TabStripType::kHorizontal) {
      const int leading_margin = GetHorizontalTabStripLeadingMargin(params);
      tabstrip_bounds = GetBoundsWithExclusion(
          params, views().horizontal_tab_strip_region_view, leading_margin);
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));
      needs_exclusion = false;
    }
    layout.AddChild(views().horizontal_tab_strip_region_view, tabstrip_bounds,
                    layout_data_->tab_strip_type == TabStripType::kHorizontal);
  }

  // This will be the area next to the vertical tab strip (if present) that will
  // be targeted for the content area when animating. It allows other elements
  // to "fly over" the contents.
  bool clip_content_for_animation = false;
  gfx::Rect unclipped_contents_region = params.visual_client_area;

  // Lay out vertical tab strip if visible.
  int collapsed_vertical_tab_strip_adjustment = 0;
  const VerticalTabStripAnimation& vertical_tab_strip_animation =
      layout_data_->vertical_tab_strip_animation;
  gfx::Rect vertical_tab_strip_bounds;
  if (IsParentedTo(views().vertical_tab_strip_region_view,
                   views().browser_view)) {
    if (layout_data_->tab_strip_type == TabStripType::kVertical) {
      if (vertical_tab_strip_animation.top_offset > 0) {
        collapsed_vertical_tab_strip_adjustment =
            horizontal_layout.vertical_tab_strip_width;
      }

      const int vertical_tab_strip_hover_width =
          std::max(0, tabs::kVerticalTabStripDefaultUncollapsedWidth -
                          horizontal_layout.vertical_tab_strip_width) *
          vertical_tab_strip_animation.expand_on_hover;

      int vertical_tab_strip_width =
          horizontal_layout.vertical_tab_strip_width +
          vertical_tab_strip_hover_width;

      // The overall width during an expand must change monotonically.
      if (vertical_tab_strip_animation.current_motion ==
          TabStripAnimations::kExpand) {
        const int target_width =
            views().vertical_tab_strip_region_view->uncollapsed_width();
        if (last_vertical_tab_strip_width_ > target_width) {
          vertical_tab_strip_width =
              std::clamp(vertical_tab_strip_width, target_width,
                         last_vertical_tab_strip_width_);
        } else {
          vertical_tab_strip_width =
              std::clamp(vertical_tab_strip_width,
                         last_vertical_tab_strip_width_, target_width);
        }
      } else {
        last_vertical_tab_strip_width_ = vertical_tab_strip_width;
      }

      vertical_tab_strip_bounds =
          gfx::Rect(params.visual_client_area.x(),
                    params.visual_client_area.y() +
                        vertical_tab_strip_animation.top_offset,
                    vertical_tab_strip_width,
                    params.visual_client_area.height() -
                        vertical_tab_strip_animation.top_offset);

      if (layout_data_->window_state == WindowState::kFullscreenWithToolbar &&
          layout_data_->separator_info.adjust_for_missing_separator) {
        vertical_tab_strip_bounds.Inset(
            gfx::Insets::TLBR(views::Separator::kThickness, 0, 0, 0));
      }

      int inset_amount = horizontal_layout.vertical_tab_strip_width;
      if (adjust_for_cracking) {
        inset_amount -= 1;
      }
      params.InsetHorizontal(inset_amount, /*leading=*/true);

      // Let the vertical tab strip animate out over the content.
      if (vertical_tab_strip_animation.current_motion) {
        clip_content_for_animation =
            vertical_tab_strip_animation.current_motion ==
                TabStripAnimations::kExpand ||
            vertical_tab_strip_animation.current_motion ==
                TabStripAnimations::kCollapse;
        unclipped_contents_region.Inset(gfx::Insets::TLBR(
            0, VerticalTabStripRegionView::kCollapsedWidth, 0, 0));
      } else {
        unclipped_contents_region.Inset(gfx::Insets::TLBR(
            0, horizontal_layout.vertical_tab_strip_width, 0, 0));
      }
    }
    layout.AddChild(views().vertical_tab_strip_region_view,
                    vertical_tab_strip_bounds,
                    layout_data_->tab_strip_type == TabStripType::kVertical);
  }

  // Position the vertical tabstrip top corner.
  if (IsParentedTo(views().vertical_tab_strip_top_corner,
                   views().browser_view)) {
    gfx::Rect corner_bounds;
    const bool top_corner_visible =
        layout_data_->tab_strip_type == TabStripType::kVertical &&
        vertical_tab_strip_animation.top_corner > 0.0;

    // The top corner is drawn when the tabstrip goes all the way to the top.
    if (top_corner_visible) {
      auto preferred =
          views().vertical_tab_strip_top_corner->GetPreferredSize();
      preferred.set_width(base::ClampCeil(
          preferred.width() * vertical_tab_strip_animation.top_corner));
      corner_bounds =
          gfx::Rect(vertical_tab_strip_bounds.top_right(), preferred);
      corner_bounds.Outset(
          gfx::Outsets::TLBR(0, views::Separator::kThickness, 0, 0));
    }
    layout.AddChild(views().vertical_tab_strip_top_corner, corner_bounds,
                    top_corner_visible);
  }

  // Position the vertical tabstrip bottom corner.
  if (IsParentedTo(views().vertical_tab_strip_bottom_corner,
                   views().browser_view)) {
    gfx::Rect corner_bounds;
    const bool bottom_corner_visible =
        layout_data_->tab_strip_type == TabStripType::kVertical &&
        vertical_tab_strip_animation.bottom_corner > 0.0;
    if (bottom_corner_visible) {
      auto preferred =
          views().vertical_tab_strip_bottom_corner->GetPreferredSize();
      preferred.set_width(base::ClampCeil(
          preferred.width() * vertical_tab_strip_animation.bottom_corner));
      corner_bounds =
          gfx::Rect(vertical_tab_strip_bounds.right(),
                    vertical_tab_strip_bounds.bottom() - preferred.height(),
                    preferred.width(), preferred.height());
      corner_bounds.Outset(
          gfx::Outsets::TLBR(0, views::Separator::kThickness, 0, 0));
    }
    layout.AddChild(views().vertical_tab_strip_bottom_corner, corner_bounds,
                    bottom_corner_visible);
  }

  // TODO(crbug.com/469425263): Ensure correct layout calculations for the
  // Project Panel Container.
  if (IsParentedToAndVisible(views().projects_panel_container,
                             views().browser_view)) {
    int target_width = projects_panel::kProjectsPanelMinWidth;
    bool projects_panel_should_appear_elevated = true;
    if (layout_data_->tab_strip_type == TabStripType::kVertical) {
      projects_panel_should_appear_elevated =
          horizontal_layout.vertical_tab_strip_width <
          projects_panel::kProjectsPanelMinWidth;
      if (!projects_panel_should_appear_elevated) {
        target_width = std::max(target_width - views::Separator::kThickness,
                                horizontal_layout.vertical_tab_strip_width -
                                    views::Separator::kThickness);
      }
    }
    views().projects_panel_container->SetTargetWidth(target_width);
    views().projects_panel_container->SetIsElevated(
        projects_panel_should_appear_elevated);

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
  ProposedLayout* main_background_layout = nullptr;
  if (IsParentedTo(views().main_background_region, views().browser_view)) {
    gfx::Rect main_background_bounds;
    if (layout_data_->separator_info.adjust_for_missing_separator) {
      // In the future this should just be a 1px shim to avoid covering
      // transparency in the frame/tab strip, instead of extending the entire
      // background to the window edge.
      main_background_bounds = gfx::Rect(
          browser_params.visual_client_area.x(), params.visual_client_area.y(),
          browser_params.visual_client_area.width(),
          params.visual_client_area.height());
    } else {
      main_background_bounds = params.visual_client_area;
    }
    if (adjust_for_cracking &&
        main_background_bounds.y() > browser_params.visual_client_area.y()) {
      main_background_bounds.Outset(gfx::Outsets::TLBR(1, 0, 0, 0));
    }
    const bool show_main_background =
        horizontal_layout.has_side_panel() || adjust_for_cracking;
    main_background_layout =
        &layout.AddChild(views().main_background_region, main_background_bounds,
                         show_main_background);
  }

  // Lay out toolbar-height side panel.
  bool side_panel_leading = false;
  const double side_panel_reveal_amount =
      horizontal_layout.has_side_panel()
          ? views().side_panel->GetAnimationValue()
          : 0.0;
  bool side_panel_is_animating = false;
  if (horizontal_layout.has_side_panel()) {
    const SidePanel* const side_panel = views().side_panel;
    side_panel_leading = side_panel->IsRightAligned() == base::i18n::IsRTL();

    if (side_panel_reveal_amount < 1.0) {
      clip_content_for_animation = true;
      side_panel_is_animating = true;
    }

    // Not all of the width may be visible on the screen.
    const int target_width = horizontal_layout.side_panel_width;
    const int visible_width =
        base::ClampFloor(target_width * side_panel_reveal_amount);

    // Add `container_inset_padding` to the top of the side panel to separate it
    // from the horizontal tab strip. SidePanel draws the top on top of the top
    // content separator and some units of the toolbar by default, which is not
    // needed for the side panel.
    const int top = params.visual_client_area.y() +
                    (layout_data_->separator_info.side_panel_top_padding
                         ? horizontal_layout.side_panel_padding
                         : 0);
    gfx::Rect side_panel_bounds(
        side_panel_leading
            ? params.visual_client_area.x() - (target_width - visible_width)
            : params.visual_client_area.right() - visible_width,
        top, target_width, params.visual_client_area.bottom() - top);
    layout.AddChild(views().side_panel, side_panel_bounds);

    // Lay out the animating contents.
    if (IsParentedToAndVisible(views().side_panel_animation_content,
                               views().browser_view)) {
      gfx::Rect side_panel_final_bounds(
          side_panel_leading ? params.visual_client_area.x()
                             : params.visual_client_area.right() - target_width,
          side_panel_bounds.y(), target_width, side_panel_bounds.height());
      gfx::Rect side_panel_animation_content_bounds =
          views().side_panel->GetContentAnimationBounds(
              side_panel_final_bounds);

      layout.AddChild(views().side_panel_animation_content,
                      side_panel_animation_content_bounds);
    }

    params.InsetHorizontal(visible_width, side_panel_leading);
    if (side_panel_reveal_amount == 1.0) {
      InsetHorizontal(unclipped_contents_region, visible_width,
                      side_panel_leading);
    }
  }

  gfx::Insets shadow_overlay_insets;
  if (layout_data_->separator_info.shadow_box) {
    // As the side panel animates in, the main panel shrinks and moves over to
    // accommodate the panel.
    const int scaled_main_area_padding = base::ClampRound(
        side_panel_reveal_amount * horizontal_layout.side_panel_padding);
    const int scaled_top_padding =
        layout_data_->separator_info.side_panel_top_padding
            ? scaled_main_area_padding
            : views::Separator::kThickness;
    shadow_overlay_insets = gfx::Insets::TLBR(
        scaled_top_padding, side_panel_leading ? 0 : scaled_main_area_padding,
        scaled_main_area_padding,
        side_panel_leading ? scaled_main_area_padding : 0);
    params.Inset(shadow_overlay_insets);
    if (!side_panel_is_animating) {
      unclipped_contents_region.Inset(shadow_overlay_insets);
    }
  }

  // Lay out the shadow overlay.
  layout.AddChild(views().main_shadow_overlay, params.visual_client_area,
                  layout_data_->separator_info.shadow_box);

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
  const bool infobar_visible = delegate().IsInfobarVisible();
  if (IsParentedTo(views().infobar_container, views().browser_view)) {
    gfx::Rect infobar_bounds;
    if (infobar_visible) {
      // Infobars slide down with top container reveal, but not when they're in
      // the toolbar-height side panel shadow box. This is because they only
      // slide down when they are visually adjacent to the toolbar/bookmarks
      // bar.
      const int additional_offset = layout_data_->separator_info.shadow_box
                                        ? 0
                                        : delegate().GetExtraInfobarOffset();
      const int infobar_height =
          views().infobar_container->GetPreferredSize().height();
      // The content starts below the infobar, but is not affected by the
      // extra offset from the toolbar reveal animation. The infobar slides
      // down to stay visible below the revealed toolbar, while the content
      // stays in place. See https://crbug.com/40278831.
      infobar_bounds =
          gfx::Rect(params.visual_client_area.x(),
                    params.visual_client_area.y() + additional_offset,
                    params.visual_client_area.width(), infobar_height);
      params.SetTop(params.visual_client_area.y() + infobar_height);
    }
    layout.AddChild(views().infobar_container, infobar_bounds, infobar_visible);
  }

  // Top separator is unnecessary when already in the shadow box; this is
  // especially obvious in split view, where turning the separator off provides
  // the required top padding.
  const auto& separator_info = layout_data_->separator_info;
  views().multi_contents_view->SetShouldShowTopSeparator(
      separator_info.multi_contents_separator);

  // Updating the top, left and right insets for contents container view when
  // in split view. This is dependent on a number of other browser states.
  if (horizontal_layout.is_split_view) {
    const bool include_top_inset =
        layout_data_->window_state == WindowState::kFullscreen ||
        infobar_visible;
    const bool include_leading_inset =
        separator_info.shadow_box ||
        !(horizontal_layout.has_side_panel() && side_panel_leading);
    const bool include_trailing_inset =
        separator_info.shadow_box ||
        !(horizontal_layout.has_side_panel() && !side_panel_leading);
    const int default_inset = MultiContentsView::kSplitViewContentInset;
    const int panel_side_inset =
        base::ClampRound((1.0 - side_panel_reveal_amount) * default_inset);

    const gfx::Insets contents_insets = gfx::Insets::TLBR(
        include_top_inset ? default_inset : 0,
        include_leading_inset ? default_inset : panel_side_inset, default_inset,
        include_trailing_inset ? default_inset : panel_side_inset);

    views().multi_contents_view->SetSplitViewInsets(contents_insets);
  }

  // Update the multi-contents view about if we will be animating content
  // bounds. This is to make optimizations during animations e.g. avoid
  // repositioning status bubble.
  views().multi_contents_view->SetIsAnimatingContent(
      side_panel_is_animating || vertical_tab_strip_animation.current_motion);

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
    // However, do not let this go past the edge of the allowed area.
    content_left =
        std::max(content_left, browser_params.visual_client_area.x());
    content_right =
        std::min(content_right, browser_params.visual_client_area.right());
  }
  auto& contents_layout =
      layout.AddChild(views().contents_container,
                      gfx::Rect(content_left, params.visual_client_area.y(),
                                content_right - content_left,
                                params.visual_client_area.height()));

  // Maybe expand and clip the web contents to avoid issues during animation.
  if (features::UseSidePanelFlyoverAnimation()) {
    if (clip_content_for_animation) {
      // Vertical span is whatever it is, minus any shadow overlay insets.
      unclipped_contents_region.set_y(contents_layout.bounds.y());
      unclipped_contents_region.set_height(contents_layout.bounds.height());
      if (side_panel_is_animating) {
        // During animation, want the vertical extent of the contents region to
        // be the exact extent that the contents will reach in the fully-closed
        // state. This means adjusting for the shadow box insets *and* (in some
        // cases) the addition of a separator.
        int top_inset = shadow_overlay_insets.top();
        if (layout_data_->separator_info.adjust_for_missing_separator) {
          top_inset = std::max(0, top_inset - views::Separator::kThickness);
        }
        unclipped_contents_region.Outset(gfx::Outsets::TLBR(
            top_inset, 0, shadow_overlay_insets.bottom(), 0));
      }

      // Avoid cases where these areas are somehow misaligned (shouldn't happen,
      // but want to avoid visual artifacts if they are).
      contents_layout.bounds.Intersect(unclipped_contents_region);
      const auto clip_insets =
          unclipped_contents_region.InsetsFrom(contents_layout.bounds);

      // Set the target size of the contents.
      views().multi_contents_view->SetTargetContentBounds(
          MultiContentsView::TargetContentBounds(
              unclipped_contents_region.size(), clip_insets));

      // Paint the main background during animations when clipping is
      // required. This prevents "cracking" at the edge of the contents area
      // as the clip region is manipulated.
      if (main_background_layout) {
        main_background_layout->visibility = true;
      }
    } else {
      views().multi_contents_view->SetTargetContentBounds(std::nullopt);
    }
  }

  // Make final visual adjustments required for child views to paint.
  if (layout_data_->tab_strip_type == TabStripType::kVertical) {
    // Need to know the toolbar height relative to the tabstrip, so that
    // vertical tabstrip elements can align with the toolbar.
    const auto toolbar_bounds =
        layout.GetBoundsFor(views().toolbar, views().browser_view);
    const auto tabstrip_bounds = layout.GetBoundsFor(
        views().vertical_tab_strip_region_view, views().browser_view);
    CHECK(tabstrip_bounds);

    int toolbar_height = 0;
    int caption_button_width = 0;

    // If the toolbar is not in the browser, then the exclusion isn't either.
    if (toolbar_bounds) {
      // Calculate the toolbar height adjacent to the tabstrip. This will be
      // zero if the toolbar is in e.g. an immersive mode overlay, or is not
      // aligned with the tabstrip (which can happen in collapsed mode with
      // leading caption buttons).
      toolbar_height = toolbar_bounds->bottom() - tabstrip_bounds->y();

      caption_button_width =
          base::ClampCeil(browser_params.leading_exclusion.content.width()) -
          tabstrip_bounds->x();
    }

    views().vertical_tab_strip_region_view->SetToolbarHeightForLayout(
        std::max(0, toolbar_height));
    views().vertical_tab_strip_region_view->SetCaptionButtonWidthForLayout(
        std::max(0, caption_button_width));

    const int padding =
        GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);
    const int target_width =
        views().vertical_tab_strip_region_view->uncollapsed_width();
    const bool will_wrap_at_destination =
        views().vertical_tab_strip_region_view->WillWrapDueToOverflow(
            target_width - 2 * padding);

    views().vertical_tab_strip_region_view->SetIsExitingExpandOnHoverForLayout(
        vertical_tab_strip_animation.current_motion &&
        vertical_tab_strip_animation.expand_on_hover > 0.0 &&
        vertical_tab_strip_animation.top_offset > 0 &&
        will_wrap_at_destination);
  }

  return layout;
}

gfx::Rect BrowserViewTabbedLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    BrowserLayoutParams params,
    bool needs_exclusion) const {
  const int original_top = params.visual_client_area.y();

  if (IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().top_container)) {
    gfx::Rect tabstrip_bounds;
    if (layout_data_->tab_strip_type == TabStripType::kHorizontal) {
      const int leading_margin = GetHorizontalTabStripLeadingMargin(params);
      tabstrip_bounds = GetBoundsWithExclusion(
          params, views().horizontal_tab_strip_region_view, leading_margin);
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));
      needs_exclusion = false;
    }
    layout.AddChild(views().horizontal_tab_strip_region_view, tabstrip_bounds,
                    layout_data_->tab_strip_type == TabStripType::kHorizontal);
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

  // Maybe show the separator in the top container.
  if (IsParentedTo(views().top_container_separator, views().top_container)) {
    gfx::Rect separator_bounds;
    if (layout_data_->separator_info.top_container_separator) {
      separator_bounds = gfx::Rect(
          params.visual_client_area.x(), params.visual_client_area.y(),
          params.visual_client_area.width(),
          views().top_container_separator->GetPreferredSize().height());
      params.SetTop(separator_bounds.bottom());
    }
    layout.AddChild(views().top_container_separator, separator_bounds,
                    layout_data_->separator_info.top_container_separator);
  }

  return gfx::Rect(params.visual_client_area.x(), original_top,
                   params.visual_client_area.width(),
                   params.visual_client_area.y() - original_top);
}

void BrowserViewTabbedLayoutImpl::ConfigureTopContainerBackground(
    const BrowserLayoutParams& params,
    CustomCornersBackground* background) {
  CHECK(layout_data_);
  // The top container always draws an opaque background in tabbed browser mode
  // to avoid cracking between visual elements.
  background->SetVisible(true);

  // In immersive mode and ChromeOS touch UI mode, the top Chrome UI is
  // parented to the `top_container()` and the frame header is not visible.
  // In these cases, the top container's background color should match the
  // frame color to ensure visual consistency.
  if (GetTabStripType() == TabStripType::kHorizontal &&
      IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().top_container)) {
    background->SetPrimaryColor(ui::kColorFrameActive);
  } else {
    background->SetPrimaryColor(CustomCornersBackground::ToolbarTheme());
  }

  // By default, this is just a flat background.
  CustomCornersBackground::Corners corners;

  // Rounded corners are drawn in vertical tab strip mode when not maximized or
  // fullscreen.
  if (layout_data_->tab_strip_type == TabStripType::kVertical &&
      layout_data_->window_state == WindowState::kNormal) {
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

void BrowserViewTabbedLayoutImpl::DoPreLayoutComputations(
    const BrowserLayoutParams& params) {
  layout_data_ = std::make_unique<TransientLayoutData>(params);
  layout_data_->window_state = delegate().GetBrowserWindowState();
  layout_data_->tab_strip_type = GetTabStripType();
  layout_data_->horizontal_layout =
      CalculateHorizontalLayout(layout_data_->revised_params);
  layout_data_->vertical_tab_strip_animation =
      CalculateVerticalTabStripAnimation();
  layout_data_->separator_info = CalculateSeparatorInfo();
}

void BrowserViewTabbedLayoutImpl::DoPostLayoutVisualAdjustments(
    const BrowserLayoutParams& params) {
  // Want to cut the vertical tabstrip and its decorations out of the top
  // container in transparency mode.
  std::vector<const views::View*> top_container_cutout_views;
  std::vector<const views::View*> main_background_cutout_views;

  // Set vertical tabstrip corners.
  if (layout_data_->tab_strip_type == TabStripType::kVertical) {
    // Vertical tabstrip goes all the way to the top of the window if it is not
    // collapsed or there are no caption buttons on the leading edge.
    const VerticalTabStripAnimation& animation =
        layout_data_->vertical_tab_strip_animation;
    auto* const vertical_tabs_background =
        views()
            .vertical_tab_strip_region_view->background()
            ->AsA<CustomCornersBackground>();
    CHECK(vertical_tabs_background)
        << "Expected vertical tab strip to have a CustomCornersBackground.";

    if (features::IsGlassFrameEnabled()) {
      float background_alpha = 0.0f;
      if (animation.tab_strip_width != 0.0) {
        background_alpha = 1.0f - animation.tab_strip_width;
      } else {
        background_alpha =
            delegate().IsVerticalTabStripCollapsed() ? 1.0f : 0.0f;
      }
      vertical_tabs_background->SetAlpha(background_alpha);
    }

    // Ensure that corners of the window remain rounded.
    CustomCornersBackground::Corners vertical_tabs_corners;
    if (layout_data_->window_state == WindowState::kNormal) {
      if (animation.top_offset == 0) {
        vertical_tabs_corners.upper_leading =
            vertical_tabs_background->GetWindowCorner(/*upper=*/true);
      }
      vertical_tabs_corners.lower_leading =
          vertical_tabs_background->GetWindowCorner(/*upper=*/false);
    }

    // When the vertical tabs are below the toolbar but next to the bookmarks
    // bar, draw a curved corner.
    if (animation.top_corner < 0.0) {
      vertical_tabs_corners.upper_trailing.type =
          views().vertical_tab_strip_region_view->is_expanded_on_hover()
              ? CustomCornersBackground::CornerType::kRounded
              : CustomCornersBackground::CornerType::kRoundedWithBackground;
      vertical_tabs_corners.upper_trailing.radius = base::ClampRound(
          vertical_tabs_background->default_radius() * -animation.top_corner);
    }

    // When the vertical tabs are expanded for hover, it may have a concave
    // corner.
    double vertical_tabs_bottom_corner_amount = 0.0;
    int vertical_tabs_bottom_corner_size = 0;
    if (animation.bottom_corner < 0.0) {
      vertical_tabs_bottom_corner_amount = -animation.bottom_corner;
      vertical_tabs_corners.lower_trailing.type =
          CustomCornersBackground::CornerType::kRounded;
      vertical_tabs_bottom_corner_size =
          base::ClampRound(vertical_tabs_background->default_radius() *
                           vertical_tabs_bottom_corner_amount);
      vertical_tabs_corners.lower_trailing.radius =
          vertical_tabs_bottom_corner_size;
    }

    vertical_tabs_background->SetCorners(vertical_tabs_corners);

    // When the projects panel is animating open or closed and does not appear
    // elevated, the background of vertical tabs should fade to match the
    // background color of the panel.
    if (delegate().IsProjectsPanelVisible()) {
      CustomFloatingCorner* const vertical_tabs_top_corner =
          views().vertical_tab_strip_top_corner;
      CustomFloatingCorner* const vertical_tabs_bottom_corner =
          views().vertical_tab_strip_bottom_corner;
      if (!views().projects_panel_container->is_elevated()) {
        auto projects_panel_reveal_amount =
            views().projects_panel_container->GetResizeAnimationValue();
        CustomCorners::FadeBackground const fade_background{
            .color = projects_panel::kProjectsPanelBackgroundColor,
            .opacity = static_cast<float>(projects_panel_reveal_amount)};
        vertical_tabs_background->SetFadeBackground(fade_background);
        vertical_tabs_top_corner->SetFadeBackground(fade_background);
        vertical_tabs_bottom_corner->SetFadeBackground(fade_background);
      } else {
        vertical_tabs_background->SetFadeBackground(std::nullopt);
        vertical_tabs_top_corner->SetFadeBackground(std::nullopt);
        vertical_tabs_bottom_corner->SetFadeBackground(std::nullopt);
      }
    }

    // Apply shadow for expand-on-hover.
    auto* const shadow_frame =
        views().vertical_tab_strip_region_view->shadow_frame();
    if (vertical_tabs_bottom_corner_amount > 0.0) {
      shadow_frame->SetShadowCornerRadius(vertical_tabs_bottom_corner_size);
      shadow_frame->SetShadowOpacity(vertical_tabs_bottom_corner_amount);
      shadow_frame->SetShadowVisible(true);
    } else {
      shadow_frame->SetShadowVisible(false);
    }

    CustomCornersBackground::Outline vertical_tabs_outline;
    vertical_tabs_outline.color = kColorVerticalTabStripShadow;
    // Vertical tabs outline fades partially during expand-on-hover to be
    // replaced with shadow.
    vertical_tabs_outline.opacity =
        1.0 - kVerticalTabStripOutlineFadeOnHover *
                  vertical_tabs_bottom_corner_amount;
    // Vertical tabs outline always draws trailing edge.
    vertical_tabs_outline.trailing = true;
    // Top edge is drawn when the tabstrip is not flush with the edge of the
    // screen, or with a visual element that provides a natural border.
    if (animation.expand_on_hover || animation.top_corner < 0.0 ||
        (animation.top_corner == 0.0 && !params.leading_exclusion.IsEmpty())) {
      vertical_tabs_outline.top = true;
    }
    if (animation.expand_on_hover) {
      vertical_tabs_outline.bottom = true;
    }
    vertical_tabs_background->SetOutline(vertical_tabs_outline);

    // Cut the vertical tab strip out of the top container.
    top_container_cutout_views.push_back(
        views().vertical_tab_strip_region_view);
    if (animation.top_corner > 0) {
      top_container_cutout_views.push_back(
          views().vertical_tab_strip_top_corner);
    }
    main_background_cutout_views = top_container_cutout_views;
    if (animation.bottom_corner > 0) {
      main_background_cutout_views.push_back(
          views().vertical_tab_strip_bottom_corner);
    }
  }

  if (!is_fullscreen(layout_data_->window_state) &&
      features::IsGlassFrameEnabled()) {
    // Do the top container cutouts.
    views()
        .top_container->background()
        ->AsA<CustomCornersBackground>()
        ->SetCutoutFrom(top_container_cutout_views);
  }

  // Set toolbar corners.
  auto* const toolbar_background =
      views().toolbar->background()->AsA<CustomCornersBackground>();
  CHECK(toolbar_background)
      << "Expected toolbar to have a CustomCornersBackground.";
  CustomCornersBackground::Corners toolbar_corners;
  switch (layout_data_->tab_strip_type) {
    case TabStripType::kHorizontal: {
      const gfx::Rect toolbar_bounds = views().toolbar->GetBoundsInScreen();
      const gfx::Rect tabstrip_bounds =
          views().horizontal_tab_strip_region_view->GetBoundsInScreen();
      if (toolbar_bounds.y() <= tabstrip_bounds.bottom()) {
        // Trailing curve is always shown for normal horizontal tabstrip when
        // the two are vertically adjacent.
        toolbar_corners.upper_trailing.type =
            CustomCornersBackground::CornerType::kRoundedWithBackground;

        // If there is anything on the leading side or the first tab is not
        // selected, then the corner radius is shown, otherwise we hide the
        // corner radius. (Don't show if the left edges don't line up.)
        if (!delegate().IsActiveTabAtLeadingWindowEdge() &&
            toolbar_bounds.x() <= tabstrip_bounds.x()) {
          toolbar_corners.upper_leading.type =
              CustomCornersBackground::CornerType::kRoundedWithBackground;
        }
      }
      break;
    }
    case TabStripType::kVertical: {
      // Curve trailing corner when it becomes the top trailing corner of the
      // browser.
      if (layout_data_->window_state == WindowState::kNormal &&
          IsParentedTo(views().top_container, views().browser_view) &&
          params.trailing_exclusion.IsEmpty()) {
        toolbar_corners.upper_trailing =
            toolbar_background->GetWindowCorner(/*upper=*/true);
      }
      break;
    }
    default:
      // This can happen in content fullscreen on Mac, but otherwise doesn't
      // happen.
      break;
  }
  toolbar_background->SetCorners(toolbar_corners);

  if (views().main_background_region &&
      views().main_background_region->GetVisible()) {
    auto* const background = views()
                                 .main_background_region->background()
                                 ->AsA<CustomCornersBackground>();
    CHECK(background)
        << "Expected main background region to have a CustomCornersBackground.";

    // Do the main area cutouts.
    if (features::IsGlassFrameEnabled()) {
      background->SetCutoutFrom(main_background_cutout_views);
    }

    CustomCornersBackground::Corners main_background_corners;

    // Frame-colored corners are shown at the top in horizontal tabstrip mode.
    // This doesn't apply in fullscreen, as the tabstrip is not in the window.
    if (layout_data_->tab_strip_type == TabStripType::kHorizontal &&
        IsParentedTo(views().horizontal_tab_strip_region_view,
                     views().browser_view)) {
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
    if (layout_data_->window_state == WindowState::kNormal) {
      if (layout_data_->tab_strip_type != TabStripType::kVertical) {
        main_background_corners.lower_leading =
            background->GetWindowCorner(/*upper=*/false);
      }
      main_background_corners.lower_trailing =
          background->GetWindowCorner(/*upper=*/false);
    }

    background->SetCorners(main_background_corners);
  }
}

void BrowserViewTabbedLayoutImpl::DoPostLayoutCleanup() {
  layout_data_.reset();
}
