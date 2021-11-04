// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_UTILS_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"

namespace content {
class WebContents;
}  // namespace content

// Sets the |x| and |width| components of |popup_bounds| as the x-coordinate
// of the starting point and the width of the popup, taking into account the
// direction it's supposed to grow (either to the left or to the right).
// Components |y| and |height| of |popup_bounds| are not changed.
// TODO(crbug.com/1247645): Rename content_area_bounds to max_bounding_box.
void CalculatePopupXAndWidth(int popup_preferred_width,
                             const gfx::Rect& content_area_bounds,
                             const gfx::Rect& element_bounds,
                             bool is_rtl,
                             gfx::Rect* popup_bounds);

// Sets the |width| and |x| component of |popup_bounds|. Without
// additional constraints, in LTR the pop-up's is left-aligned with the
// horizontal center of |element_bounds|. If the space available to the
// right side it insufficient to achieve the |popup_preferred_width|, the left
// border of the pop-up is moved further to the left.
// In RTL the logic is revered. The pop-up is right-aligned with the horizontal
// center of |element_bounds|. If the space to the left is insufficient, the
// pop-up will grow towards the right.
// TODO(crbug.com/1247645): Rename content_area_bounds to max_bounding_box.
void CalculatePopupXAndWidthHorizontallyCentered(
    int popup_preferred_width,
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    bool is_rtl,
    gfx::Rect* popup_bounds);

// Sets the |y| and |height| components of |popup_bounds| as the y-coordinate
// of the starting point and the height of the popup, taking into account the
// direction it's supposed to grow (either up or down). Components |x| and
// |width| of |popup_bounds| are not changed.
// TODO(crbug.com/1247645): Rename content_area_bounds to max_bounding_box.
void CalculatePopupYAndHeight(int popup_preferred_height,
                              const gfx::Rect& content_area_bounds,
                              const gfx::Rect& element_bounds,
                              gfx::Rect* popup_bounds);

// Convenience method which handles both the vertical and horizontal bounds
// and returns a new Rect. If |horizontally_centered| is true, the pop-up is
// horizontally aligned with the center of |element_bounds|.
// TODO(crbug.com/1247645): Rename content_area_bounds to max_bounding_box.
gfx::Rect CalculatePopupBounds(const gfx::Size& desired_size,
                               const gfx::Rect& content_area_bounds,
                               const gfx::Rect& element_bounds,
                               bool is_rtl,
                               bool horizontally_centered);

// Returns whether there is enough height within |content_area_bounds| above or
// below |element_bounds| to display |item_height|, and that the first dropdown
// item will actually be visible within the bounds of the content area.
// TODO(crbug.com/1247645): Rename content_area_bounds to max_bounding_box.
bool CanShowDropdownHere(int item_height,
                         const gfx::Rect& content_area_bounds,
                         const gfx::Rect& element_bounds);

// Returns whether there is any open prompt in |web_contents| with bounds that
// overlap |screen_bounds|.
bool BoundsOverlapWithAnyOpenPrompt(const gfx::Rect& screen_bounds,
                                    content::WebContents* web_contents);

// Returns the total vertical space to place a on |content_area_bounds| on a
// specific |side| of the |element_bounds|.
int GetAvailableVerticalSpaceOnSideOfElement(
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    views::BubbleArrowSide side);

// Returns the total horizontal space to place on |content_area_bounds| on a
// specific |side| of the |element_bounds|.
int GetAvailableHorizontalSpaceOnSideOfElement(
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    views::BubbleArrowSide side);

// Returns true if there is enough space to place the bubble with
// |bubble_preferred_size| plus an additional |spacing| on a specific |side| of
// the |element_bounds| in the |content_area_bounds|. |spacing| defines the
// number of additional pixels the bubble should be displaced from the element.
bool IsBubblePlaceableOnSideOfElement(const gfx::Rect& content_area_bounds,
                                      const gfx::Rect& element_bounds,
                                      const gfx::Size& bubble_preferred_size,
                                      int spacing,
                                      views::BubbleArrowSide side);

// Returns the first side within this order kTop, kBottom, kLeft, kRight, for
// which which the bubble with a |bubble_preferred_size| fits on the side of the
// |element_bounds| in the |content_area_bounds| taking the arrow length into
// account. If neither side fits, the function returns kBottom.
views::BubbleArrowSide GetOptimalBubbleArrowSide(
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    const gfx::Size& bubble_preferred_size);

// Returns whether there is an open permissions prompt in |web_contents| with
// bounds that overlap |screen_bounds|.
bool BoundsOverlapWithOpenPermissionsPrompt(const gfx::Rect& screen_bounds,
                                            content::WebContents* web_contents);

// Returns whether a popup may vertically exceed the bounds of |web_contents|.
// This is permitted for extension popups. Here we only enforce that the
// autofill popup is at least attached to the extension popup (or overlaps the
// extension popup) and stays within the bounds of the browser window.
bool PopupMayExceedContentAreaBounds(content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_UTILS_H_
