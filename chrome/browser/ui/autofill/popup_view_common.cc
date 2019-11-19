// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_view_common.h"

#include <algorithm>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "ui/gfx/geometry/vector2d.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#else  // defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/views/widget/widget.h"
#endif  // !defined(OS_ANDROID)

namespace autofill {

namespace {

// CalculateWidthValues below ultimately determines the maximum popup width
// given the constraints of the window and the element's position within it.
// Returning a struct allows callers to reuse intermediary values produced
// during this calculation which are also relevant to positioning and sizing.
struct WidthCalculationResults {
  int right_growth_start;  // Popup start coordinate when growing to the right.
  int left_growth_end;     // Popup end coordinate when growing to the left.
  int right_available;     // Amount of space available if drawing to the right.
  int left_available;      // Amount of space available if drawing to the left.
  int max_popup_width;     // The max of |right_available| and |left_available|.
};

WidthCalculationResults CalculateWidthValues(int leftmost_available_x,
                                             int rightmost_available_x,
                                             const gfx::Rect& element_bounds) {
  WidthCalculationResults result;
  result.right_growth_start =
      std::max(leftmost_available_x,
               std::min(rightmost_available_x, element_bounds.x()));
  result.left_growth_end =
      std::max(leftmost_available_x,
               std::min(rightmost_available_x, element_bounds.right()));

  result.right_available = rightmost_available_x - result.right_growth_start;
  result.left_available = result.left_growth_end - leftmost_available_x;

  result.max_popup_width =
      std::max(result.right_available, result.left_available);
  return result;
}

// Sets the |x| and |width| components of |popup_bounds| as the x-coordinate of
// the starting point and the width of the popup, taking into account the
// direction it's supposed to grow (either to the left or to the right).
// Components |y| and |height| of |popup_bounds| are not changed.
void CalculatePopupXAndWidth(int leftmost_available_x,
                             int rightmost_available_x,
                             int popup_required_width,
                             const gfx::Rect& element_bounds,
                             bool is_rtl,
                             gfx::Rect* popup_bounds) {
  WidthCalculationResults result = CalculateWidthValues(
      leftmost_available_x, rightmost_available_x, element_bounds);

  int popup_width = std::min(popup_required_width, result.max_popup_width);

  // Prefer to grow towards the end (right for LTR, left for RTL). But if there
  // is not enough space available in the desired direction and more space in
  // the other direction, reverse it.
  bool grow_left = false;
  if (is_rtl) {
    grow_left = result.left_available >= popup_width ||
                result.left_available >= result.right_available;
  } else {
    grow_left = result.right_available < popup_width &&
                result.right_available < result.left_available;
  }

  popup_bounds->set_width(popup_width);
  popup_bounds->set_x(grow_left ? result.left_growth_end - popup_width
                                : result.right_growth_start);
}

// Sets the |y| and |height| components of |popup_bounds| as the y-coordinate of
// the starting point and the height of the popup, taking into account the
// direction it's supposed to grow (either up or down). Components |x| and
// |width| of |popup_bounds| are not changed.
void CalculatePopupYAndHeight(int topmost_available_y,
                              int bottommost_available_y,
                              int popup_required_height,
                              const gfx::Rect& element_bounds,
                              gfx::Rect* popup_bounds) {
  // Calculate the start coordinates for the popup if it is growing down or
  // the end position if it is growing up, capped to screen space.
  int top_growth_end =
      std::max(topmost_available_y,
               std::min(bottommost_available_y, element_bounds.y()));
  int bottom_growth_start =
      std::max(topmost_available_y,
               std::min(bottommost_available_y, element_bounds.bottom()));

  int top_available = top_growth_end - topmost_available_y;
  int bottom_available = bottommost_available_y - bottom_growth_start;

  if (bottom_available >= popup_required_height ||
      bottom_available >= top_available) {
    // The popup can appear below the field.
    popup_bounds->set_height(std::min(bottom_available, popup_required_height));
    popup_bounds->set_y(bottom_growth_start);
  } else {
    // The popup must appear above the field.
    popup_bounds->set_height(std::min(top_available, popup_required_height));
    popup_bounds->set_y(top_growth_end - popup_bounds->height());
  }
}

}  // namespace

void PopupViewCommon::CalculatePopupHorizontalBounds(
    int desired_width,
    const gfx::Rect& element_bounds,
    gfx::NativeView container_view,
    bool is_rtl,
    gfx::Rect* popup_bounds) {
  const gfx::Rect bounds = GetWindowBounds(container_view);
  CalculatePopupXAndWidth(/*leftmost_available_x=*/bounds.x(),
                          /*rightmost_available_x=*/bounds.x() + bounds.width(),
                          desired_width, element_bounds, is_rtl, popup_bounds);
}

void PopupViewCommon::CalculatePopupVerticalBounds(
    int desired_height,
    const gfx::Rect& element_bounds,
    gfx::NativeView container_view,
    gfx::Rect* popup_bounds) {
  const gfx::Rect window_bounds = GetWindowBounds(container_view);
  CalculatePopupYAndHeight(
      /*topmost_available_y=*/window_bounds.y(),
      /*bottommost_available_y=*/window_bounds.y() + window_bounds.height(),
      desired_height, element_bounds, popup_bounds);
}

gfx::Rect PopupViewCommon::CalculatePopupBounds(int desired_width,
                                                int desired_height,
                                                const gfx::Rect& element_bounds,
                                                gfx::NativeView container_view,
                                                bool is_rtl) {
  const gfx::Rect window_bounds = GetWindowBounds(container_view);

  gfx::Rect popup_bounds;
  CalculatePopupXAndWidth(
      /*leftmost_available_x=*/window_bounds.x(),
      /*rightmost_available_x=*/window_bounds.x() + window_bounds.width(),
      desired_width, element_bounds, is_rtl, &popup_bounds);
  CalculatePopupYAndHeight(
      /*topmost_available_y=*/window_bounds.y(),
      /*bottommost_available_y=*/window_bounds.y() + window_bounds.height(),
      desired_height, element_bounds, &popup_bounds);

  return popup_bounds;
}

gfx::Rect PopupViewCommon::GetWindowBounds(gfx::NativeView container_view) {
// The call to FindBrowserWithWindow will fail on Android, so we use
// platform-specific calls.
#if defined(OS_ANDROID)
  return container_view->GetWindowAndroid()->bounds();
#else
  views::Widget* widget =
      views::Widget::GetTopLevelWidgetForNativeView(container_view);
  if (widget)
    return widget->GetWindowBoundsInScreen();

  // If the widget is null, try to get these bounds from a browser window. This
  // is common on Mac when the window is drawn using Cocoa.
  gfx::NativeWindow window = platform_util::GetTopLevel(container_view);
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser)
    return browser->window()->GetBounds();

  // If the browser is null, simply return an empty rect. The most common reason
  // to end up here is that the NativeView has been destroyed externally, which
  // can happen at any time. This happens fairly commonly on Windows (e.g., at
  // shutdown) in particular.
  return gfx::Rect();
#endif
}

int PopupViewCommon::CalculateMaxWidth(const gfx::Rect& element_bounds,
                                       gfx::NativeView container_view) {
  const gfx::Rect window_bounds = GetWindowBounds(container_view);
  int leftmost_available_x = window_bounds.x();
  int rightmost_available_x = window_bounds.x() + window_bounds.width();
  return CalculateWidthValues(leftmost_available_x, rightmost_available_x,
                              element_bounds)
      .max_popup_width;
}

}  // namespace autofill
