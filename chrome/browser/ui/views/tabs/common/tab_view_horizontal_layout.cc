// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_view_horizontal_layout.h"

#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab/tab_title.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/view_utils.h"

namespace {
// When the content's width of the tab shrinks to below this size we should
// hide the close button on inactive tabs. Any smaller and they're too easy
// to hit on accident.
static constexpr int kMinimumContentsWidthForCloseButtons = 68;
static constexpr int kTouchMinimumContentsWidthForCloseButtons = 100;

// Additional padding of close button to the right of the tab
// indicator when `extra_alert_indicator_padding` is true.
constexpr int kTabAlertIndicatorCloseButtonPaddingAdjustmentTouchUI = 8;
constexpr int kTabAlertIndicatorCloseButtonPaddingAdjustment = 4;

// Returns the coordinate for an object of size `item_size` centered in a region
// of size `size`, biasing towards placing any extra space ahead of the object.
int Center(int size, int item_size) {
  int extra_space = size - item_size;
  // Integer division below truncates, thus effectively "rounding toward zero";
  // to always place extra space ahead of the object, we want to round towards
  // positive infinity, which means we need to bias the division only when the
  // size difference is positive.  (Adding one unconditionally will stack with
  // the truncation if `extra_space` is negative, resulting in off-by-one
  // errors.)
  if (extra_space > 0) {
    ++extra_space;
  }
  return extra_space / 2;
}
}  // namespace

TabViewHorizontalLayout::TabViewHorizontalLayout() = default;
TabViewHorizontalLayout::~TabViewHorizontalLayout() = default;

void TabViewHorizontalLayout::OnTabClosing() {
  center_icon_override_ =
      CalculateChildVisibilities(TabView().width(),
                                 TabView().GetContentsBounds().width())
          .center_icon;
}

views::ProposedLayout TabViewHorizontalLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  int width;
  if (TabView().pinned_) {
    width =
        TabView().tab_styling()->tab_style()->GetPinnedWidth(TabView().split());
  } else {
    const int preferred_width =
        TabView().tab_styling()->tab_style()->GetStandardWidth(
            TabView().split());
    const int minimum_width =
        TabView().active_
            ? TabView().tab_styling()->tab_style()->GetMinimumActiveWidth(
                  TabView().split())
            : TabView().tab_styling()->tab_style()->GetMinimumInactiveWidth();
    width =
        std::clamp(size_bounds.width().value_or(preferred_width),
                   TabView().IsClosing() ? 0 : minimum_width, preferred_width);
  }
  const int height = TabView().tab_styling()->tab_style()->GetStandardHeight();

  views::ProposedLayout layouts;
  layouts.host_size = gfx::Size(width, height);
  gfx::Rect contents_rect = gfx::Rect(layouts.host_size);
  contents_rect.Inset(TabView().GetInsets());

  auto [showing_icon, showing_alert_indicator, showing_close_button, show_title,
        center_icon, extra_alert_indicator_padding] =
      CalculateChildVisibilities(width, contents_rect.width());

  const int start = contents_rect.x();

  // Position the underline under the tab contents.
  if (TabView().glic_tab_underline_view_) {
    constexpr int kGlicUnderlineYOffset = 8;
    gfx::Rect glic_bounds =
        contents_rect + gfx::Vector2d(0, kGlicUnderlineYOffset);
    // Use the full width of the tab in order to accommodate small tab sizes
    // where the width of the contents bounds is 0.
    glic_bounds.set_x(0);
    glic_bounds.set_width(width);
    layouts.child_layouts.emplace_back(
        TabView().glic_tab_underline_view_.get(),
        TabView().glic_tab_underline_view_->GetVisible(), glic_bounds);
  }

  // The bounds for the favicon will include extra width for the attention
  // indicator, but visually it will be smaller at kFaviconSize wide.
  gfx::Rect favicon_bounds(start, contents_rect.y(), 0, 0);
  if (showing_icon) {
    if (center_icon) {
      // When centering the favicon, the favicon is allowed to escape the normal
      // contents rect.
      favicon_bounds.set_x(Center(width, gfx::kFaviconSize));
    }

    // Add space for insets outside the favicon bounds.
    favicon_bounds -= gfx::Vector2d(TabView().icon_->GetInsets().left(),
                                    TabView().icon_->GetInsets().top());
    favicon_bounds.set_size(TabView().icon_->GetPreferredSize());
  }
  layouts.child_layouts.emplace_back(TabView().icon_.get(), showing_icon,
                                     favicon_bounds);

  const int after_title_padding =
      GetLayoutConstant(LayoutConstant::kTabAfterTitlePadding);

  int close_x = contents_rect.right();
  gfx::Rect close_button_bounds;
  if (showing_close_button) {
    // The visible size is the button's hover shape size. The actual size
    // includes the border insets for the button.
    const int close_button_visible_size =
        GetLayoutConstant(LayoutConstant::kTabCloseButtonSize);
    const gfx::Size close_button_actual_size =
        TabView().close_button_->GetPreferredSize();

    // The close button is vertically centered in the contents_rect.
    const int top =
        contents_rect.y() +
        Center(contents_rect.height(), close_button_actual_size.height());

    // The visible part of the close button should be placed against the
    // right of the contents rect unless the tab is so small that it would
    // overflow the left side of the contents_rect, in that case it will be
    // placed in the middle of the tab.
    const int visible_left =
        (center_icon && features::IsTabStripDeclutterEnabled())
            ? Center(width, close_button_visible_size)
            : std::max(close_x - close_button_visible_size,
                       Center(width, close_button_visible_size));

    // Offset the new bounds rect by the extra padding in the close button.
    const int non_visible_left_padding =
        (close_button_actual_size.width() - close_button_visible_size) / 2;

    close_button_bounds =
        gfx::Rect(gfx::Point(visible_left - non_visible_left_padding, top),
                  close_button_actual_size);
    close_x = visible_left - after_title_padding;
  }
  layouts.child_layouts.emplace_back(TabView().close_button_.get(),
                                     showing_close_button, close_button_bounds);

  gfx::Rect alert_indicator_bounds;
  if (showing_alert_indicator) {
    int right = contents_rect.right();
    if (showing_close_button) {
      right = close_x;
      if (extra_alert_indicator_padding) {
        right -= ui::TouchUiController::Get()->touch_ui()
                     ? kTabAlertIndicatorCloseButtonPaddingAdjustmentTouchUI
                     : kTabAlertIndicatorCloseButtonPaddingAdjustment;
      }
    }
    const gfx::Size image_size = TabView().alert_indicator_->GetPreferredSize();
    alert_indicator_bounds = gfx::Rect(
        std::max(contents_rect.x(), right - image_size.width()),
        contents_rect.y() + Center(contents_rect.height(), image_size.height()),
        image_size.width(), image_size.height());
    if (center_icon) {
      // When centering the alert icon, it is allowed to escape the normal
      // contents rect.
      alert_indicator_bounds.set_x(
          Center(width, alert_indicator_bounds.width()));
    }
  }
  layouts.child_layouts.emplace_back(TabView().alert_indicator_.get(),
                                     showing_alert_indicator,
                                     alert_indicator_bounds);

  // Size the title to fill the remaining width and use all available height.
  gfx::Rect title_bounds;
  if (show_title) {
    int title_left = start;
    if (showing_icon) {
      // When computing the spacing from the favicon, don't count the actual
      // icon view width (which will include extra room for the alert
      // indicator), but rather the normal favicon width which is what it will
      // look like.
      const int after_favicon =
          favicon_bounds.x() + TabView().icon_->GetInsets().left() +
          gfx::kFaviconSize +
          GetLayoutConstant(LayoutConstant::kTabPreTitlePadding);
      title_left = std::max(title_left, after_favicon);
    }
    int title_right = contents_rect.right();
    if (showing_alert_indicator) {
      title_right = alert_indicator_bounds.x() - after_title_padding;
    } else if (showing_close_button) {
      // Allow the title to overlay the close button's empty border padding.
      title_right = close_x;
    }
    const int title_width = std::max(title_right - title_left, 0);
    // The Label will automatically center the font's cap height within the
    // provided vertical space.
    title_bounds = gfx::Rect(title_left, 0, title_width, height);
    show_title = title_width > 0;
  }
  layouts.child_layouts.emplace_back(TabView().title_.get(), show_title,
                                     title_bounds);

  return layouts;
}

TabViewHorizontalLayout::ChildVisibilities
TabViewHorizontalLayout::CalculateChildVisibilities(int width,
                                                    int available_width) const {
  ChildVisibilities icon_visibilities;
  auto& [showing_icon, showing_alert_indicator, showing_close_button,
         show_title, center_icon, extra_alert_indicator_padding] =
      icon_visibilities;

  // When a tab is less than its minimum inactive width it's implied that it's
  // closing, but some favicon functionality can escape the bounds of the tab
  // causing artifacts. Fix this by explicitly disabling the visibility of the
  // views in the tab if the width is such that it is collapsed.
  if (width < TabView().tab_styling()->tab_style()->GetMinimumInactiveWidth()) {
    return icon_visibilities;
  }

  const bool has_favicon = TabView().data().should_display_favicon;
  bool has_alert_icon =
      TabView().alert_indicator_->showing_alert_state().has_value();
  std::optional<tabs::TabAlert> current_alert_state =
      TabView().alert_indicator_->showing_alert_state();
  if (TabView().glic_tab_underline_view_ &&
      (current_alert_state == tabs::TabAlert::kGlicAccessing ||
       current_alert_state == tabs::TabAlert::kGlicSharing)) {
    // Tab underlines for glic multitab replace `alert_indicator_` as the
    // UI indicator for sharing. In this case, ensure the alert indicator is
    // hidden.
    has_alert_icon = false;
  }

  if (TabView().pinned_) {
    // When the tab is pinned, we can show one of the two icons; the alert icon
    // is given priority over the favicon. The close button is never shown.
    showing_alert_indicator = has_alert_icon;
    showing_icon = has_favicon && !has_alert_icon;
    showing_close_button = false;

    show_title = false;
    center_icon = true;

    // While animating to or from the pinned state, pinned tabs are rendered as
    // normal tabs. Force the extra padding on so the favicon doesn't jitter
    // left and then back right again as it resizes through layout regimes.
    extra_alert_indicator_padding = true;
    return icon_visibilities;
  }

  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  const int favicon_width = gfx::kFaviconSize;
  const int alert_icon_width =
      TabView().alert_indicator_->GetPreferredSize().width();
  // In case of touch optimized UI, the close button has an extra padding on the
  // left that needs to be considered.
  const int close_button_width =
      GetLayoutConstant(LayoutConstant::kTabCloseButtonSize) +
      GetLayoutConstant(LayoutConstant::kTabAfterTitlePadding);
  const bool large_enough_for_close_button =
      available_width >= (touch_ui ? kTouchMinimumContentsWidthForCloseButtons
                                   : kMinimumContentsWidthForCloseButtons);

  const bool declutter_eligible =
      features::IsTabStripDeclutterEnabled() &&
      !(TabView().hovered_ || TabView().HasFocus() ||
        (TabView().close_button_ && TabView().close_button_->HasFocus()));

  bool should_show_close_button = true;
#if BUILDFLAG(IS_CHROMEOS)
  // Hide tab close button for OnTask if locked. Only applicable for non-web
  // browser scenarios.
  const TabCollectionNode* collection_node = TabView().collection_node();
  if (collection_node && collection_node->GetController()) {
    should_show_close_button =
        !collection_node->GetController()->IsLockedForOnTask();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (TabView().active_) {
    if (!declutter_eligible) {
      // Close button is shown on active tabs regardless of the size.
      showing_close_button = should_show_close_button;
      if (showing_close_button) {
        available_width -= close_button_width;
      }
    }

    showing_alert_indicator =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator) {
      available_width -= alert_icon_width;
    }

    showing_icon = has_favicon && favicon_width <= available_width;
    if (showing_icon) {
      available_width -= favicon_width;
    }

    if (declutter_eligible) {
      showing_close_button =
          should_show_close_button && ((!has_alert_icon && !has_favicon) ||
                                       close_button_width <= available_width);
      if (showing_close_button) {
        available_width -= close_button_width;
      }
    }
  } else {
    showing_alert_indicator =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator) {
      available_width -= alert_icon_width;
    }

    showing_icon = has_favicon && favicon_width <= available_width;
    if (showing_icon) {
      available_width -= favicon_width;
    }
    showing_close_button =
        should_show_close_button && large_enough_for_close_button &&
        !(declutter_eligible &&
          available_width <=
              TabStyle::kTabStripDeclutterMaxTabWidthForCloseHide);
    if (showing_close_button) {
      available_width -= close_button_width;
    }

    // If no other controls are visible, show the alert icon or the favicon
    // even though we don't have enough space. We'll clip the icon in
    // PaintChildren().
    if (!showing_close_button && !showing_alert_indicator && !showing_icon) {
      showing_alert_indicator = has_alert_icon;
      showing_icon = !showing_alert_indicator && has_favicon;

      // See comments near top of function on why this conditional is here.
      if (showing_alert_indicator || showing_icon) {
        center_icon = true;
      }
    }
  }

  if (features::IsTabStripDeclutterEnabled()) {
    const int max_width_to_center_icon =
        GetLayoutConstant(LayoutConstant::kTabPreTitlePadding) +
        GetLayoutConstant(LayoutConstant::kTabAfterTitlePadding);

    const int num_icons_showing =
        showing_icon + showing_alert_indicator + showing_close_button;

    if (available_width < max_width_to_center_icon && num_icons_showing == 1) {
      center_icon = true;
    }
  }

  extra_alert_indicator_padding = showing_alert_indicator &&
                                  showing_close_button &&
                                  large_enough_for_close_button;

  if (center_icon_override_.has_value()) {
    center_icon = center_icon_override_.value();
  }

  show_title = !(features::IsTabStripDeclutterEnabled() && center_icon);

  return icon_visibilities;
}
