// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"

#include <algorithm>

#include "base/cxx17_backports.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

void CalculatePopupXAndWidthHorizontallyCentered(
    int popup_preferred_width,
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    bool is_rtl,
    gfx::Rect* popup_bounds) {
  // The preferred horizontal starting point for the pop-up is at the horizontal
  // center of the field.
  int preferred_starting_point =
      base::clamp(element_bounds.x() + (element_bounds.size().width() / 2),
                  content_area_bounds.x(), content_area_bounds.right());

  // The space available to the left and to the right.
  int space_to_right = content_area_bounds.right() - preferred_starting_point;
  int space_to_left = preferred_starting_point - content_area_bounds.x();

  // Calculate the pop-up width. This is either the preferred pop-up width, or
  // alternatively the maximum space available if there is not sufficient space
  // for the preferred width.
  int popup_width =
      std::min(popup_preferred_width, space_to_left + space_to_right);

  // Calculates the space that is available to grow into the preferred
  // direction. In RTL, this is the space to the right side of the content
  // area, in LTR this is the space to the left side of the content area.
  int space_to_grow_in_preferred_direction =
      is_rtl ? space_to_left : space_to_right;

  // Calculate how much the pop-up needs to grow into the non-preferred
  // direction.
  int amount_to_grow_in_unpreffered_direction =
      std::max(0, popup_width - space_to_grow_in_preferred_direction);

  popup_bounds->set_width(popup_width);
  if (is_rtl) {
    // Note, in RTL the |pop_up_width| must be subtracted to achieve
    // right-alignment of the pop-up with the element.
    popup_bounds->set_x(preferred_starting_point - popup_width +
                        amount_to_grow_in_unpreffered_direction);
  } else {
    popup_bounds->set_x(preferred_starting_point -
                        amount_to_grow_in_unpreffered_direction);
  }
}

void CalculatePopupXAndWidth(int popup_preferred_width,
                             const gfx::Rect& content_area_bounds,
                             const gfx::Rect& element_bounds,
                             bool is_rtl,
                             gfx::Rect* popup_bounds) {
  int right_growth_start = base::clamp(
      element_bounds.x(), content_area_bounds.x(), content_area_bounds.right());
  int left_growth_end =
      base::clamp(element_bounds.right(), content_area_bounds.x(),
                  content_area_bounds.right());

  int right_available = content_area_bounds.right() - right_growth_start;
  int left_available = left_growth_end - content_area_bounds.x();

  int popup_width = std::min(popup_preferred_width,
                             std::max(left_available, right_available));

  // Prefer to grow towards the end (right for LTR, left for RTL). But if there
  // is not enough space available in the desired direction and more space in
  // the other direction, reverse it.
  bool grow_left = false;
  if (is_rtl) {
    grow_left =
        left_available >= popup_width || left_available >= right_available;
  } else {
    grow_left =
        right_available < popup_width && right_available < left_available;
  }

  popup_bounds->set_width(popup_width);
  popup_bounds->set_x(grow_left ? left_growth_end - popup_width
                                : right_growth_start);
}

void CalculatePopupYAndHeight(int popup_preferred_height,
                              const gfx::Rect& content_area_bounds,
                              const gfx::Rect& element_bounds,
                              gfx::Rect* popup_bounds) {
  int top_growth_end = base::clamp(element_bounds.y(), content_area_bounds.y(),
                                   content_area_bounds.bottom());
  int bottom_growth_start =
      base::clamp(element_bounds.bottom(), content_area_bounds.y(),
                  content_area_bounds.bottom());

  int top_available = top_growth_end - content_area_bounds.y();
  int bottom_available = content_area_bounds.bottom() - bottom_growth_start;

  popup_bounds->set_height(popup_preferred_height);
  popup_bounds->set_y(top_growth_end);

  if (bottom_available >= popup_preferred_height ||
      bottom_available >= top_available) {
    popup_bounds->AdjustToFit(
        gfx::Rect(popup_bounds->x(), element_bounds.bottom(),
                  popup_bounds->width(), bottom_available));
  } else {
    popup_bounds->AdjustToFit(gfx::Rect(popup_bounds->x(),
                                        content_area_bounds.y(),
                                        popup_bounds->width(), top_available));
  }
}

gfx::Rect CalculatePopupBounds(const gfx::Size& desired_size,
                               const gfx::Rect& content_area_bounds,
                               const gfx::Rect& element_bounds,
                               bool is_rtl,
                               bool horizontally_centered) {
  gfx::Rect popup_bounds;

  if (horizontally_centered) {
    CalculatePopupXAndWidthHorizontallyCentered(
        desired_size.width(), content_area_bounds, element_bounds, is_rtl,
        &popup_bounds);
  } else {
    CalculatePopupXAndWidth(desired_size.width(), content_area_bounds,
                            element_bounds, is_rtl, &popup_bounds);
  }
  CalculatePopupYAndHeight(desired_size.height(), content_area_bounds,
                           element_bounds, &popup_bounds);

  return popup_bounds;
}

bool CanShowDropdownHere(int item_height,
                         const gfx::Rect& content_area_bounds,
                         const gfx::Rect& element_bounds) {
  // Ensure that at least one row of the popup will be displayed within the
  // bounds of the content area so that the user notices the presence of the
  // popup.
  bool enough_space_for_one_item_in_content_area_above_element =
      element_bounds.y() - content_area_bounds.y() >= item_height;
  bool element_top_is_within_content_area_bounds =
      element_bounds.y() >= content_area_bounds.y() &&
      element_bounds.y() < content_area_bounds.bottom();

  bool enough_space_for_one_item_in_content_area_below_element =
      content_area_bounds.bottom() - element_bounds.bottom() >= item_height;
  bool element_bottom_is_within_content_area_bounds =
      element_bounds.bottom() > content_area_bounds.y() &&
      element_bounds.bottom() <= content_area_bounds.bottom();

  return (enough_space_for_one_item_in_content_area_above_element &&
          element_top_is_within_content_area_bounds) ||
         (enough_space_for_one_item_in_content_area_below_element &&
          element_bottom_is_within_content_area_bounds);
}

bool BoundsOverlapWithAnyOpenPrompt(const gfx::Rect& screen_bounds,
                                    content::WebContents* web_contents) {
  gfx::NativeView top_level_view =
      platform_util::GetViewForWindow(web_contents->GetTopLevelNativeWindow());
  if (!top_level_view)
    return false;
  // We generally want to ensure that no prompt overlaps with |screen_bounds|.
  // It is possible, however, that a <datalist> is part of a prompt (e.g. an
  // extension popup can render a <datalist>). Therefore, we exclude the widget
  // that hosts the |web_contents| from the prompts that are considered for
  // overlaps.
  views::Widget* web_contents_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          web_contents->GetContentNativeView());

  // On Aura-based systems, prompts are siblings to the top level native window,
  // and hence we need to go one level up to start searching from the root
  // window.
  top_level_view = platform_util::GetParent(top_level_view)
                       ? platform_util::GetParent(top_level_view)
                       : top_level_view;
  views::Widget::Widgets all_widgets;
  views::Widget::GetAllChildWidgets(top_level_view, &all_widgets);
  return base::ranges::any_of(
      all_widgets, [&screen_bounds, web_contents_widget](views::Widget* w) {
        return w->IsDialogBox() &&
               w->GetWindowBoundsInScreen().Intersects(screen_bounds) &&
               w != web_contents_widget;
      });
}

bool BoundsOverlapWithOpenPermissionsPrompt(
    const gfx::Rect& screen_bounds,
    content::WebContents* web_contents) {
  views::View* const permission_bubble_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          PermissionPromptBubbleView::kPermissionPromptBubbleViewIdentifier,
          views::ElementTrackerViews::GetInstance()->GetContextForView(
              BrowserView::GetBrowserViewForBrowser(
                  chrome::FindBrowserWithWebContents(web_contents))));
  if (!permission_bubble_view)
    return false;

  return permission_bubble_view->GetWidget()
      ->GetWindowBoundsInScreen()
      .Intersects(screen_bounds);
}

bool PopupMayExceedContentAreaBounds(content::WebContents* web_contents) {
  if (!web_contents)  // May be null for tests.
    return false;
  const GURL& url = web_contents->GetLastCommittedURL();
  // Extensions may want to show <datalist> form controls whose popups cannot be
  // rendered within the bounds of an extension popup. For that reason they are
  // allow-listed to draw popups outside the boundary of the extension popup.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
        web_contents->GetContentNativeView());
    return widget && widget->GetName() == ExtensionPopup::kViewClassName;
  }
  return false;
}
