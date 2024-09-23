// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_UTILS_H_

#include "base/containers/span.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"
#include "ui/views/style/typography.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Specifies how the popup cell was selected.
enum PopupCellSelectionSource {
  // (Un)selections with no direct user input, e.g. unselection by timeout.
  kNonUserInput,
  kMouse,
  kKeyboard,
};

// Sets the `y` and `height` components of `popup_bounds` as the y-coordinate
// of the starting point and the height of the popup, taking into account the
// direction it's supposed to grow (either up or down). Components `x` and
// `width` of `popup_bounds` are not changed.
void CalculatePopupYAndHeight(int popup_preferred_height,
                              const gfx::Rect& content_area_bounds,
                              const gfx::Rect& element_bounds,
                              gfx::Rect* popup_bounds);

// Returns whether there is enough height within `content_area_bounds` above or
// below `element_bounds` to display `item_height`, and that the first dropdown
// item will actually be visible within the bounds of the content area.
bool CanShowDropdownHere(int item_height,
                         const gfx::Rect& content_area_bounds,
                         const gfx::Rect& element_bounds);

// Returns whether there is any open prompt in `web_contents` with bounds that
// overlap `screen_bounds`.
// This is unreliable on Windows because bubbles are not necessarily children of
// the root view of the current tab.
bool BoundsOverlapWithAnyOpenPrompt(const gfx::Rect& screen_bounds,
                                    content::WebContents* web_contents);

// Returns the total vertical space on `content_area_bounds` on a specific
// `side` of the `element_bounds`.
int GetAvailableVerticalSpaceOnSideOfElement(
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    views::BubbleArrowSide side);

// Returns the total horizontal space on `content_area_bounds` on a
// specific `side` of the `element_bounds`.
int GetAvailableHorizontalSpaceOnSideOfElement(
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    views::BubbleArrowSide side);

// Returns true if there is enough space to place the popup with
// `popup_preferred_size` plus an additional `spacing` on a specific
// `side` of the `element_bounds` in the `content_area_bounds`. `spacing`
// defines the number of additional pixels the popup should be displaced from
// the element.
bool IsPopupPlaceableOnSideOfElement(const gfx::Rect& content_area_bounds,
                                     const gfx::Rect& element_bounds,
                                     const gfx::Size& popup_preferred_size,
                                     int spacing,
                                     views::BubbleArrowSide side);

// Returns the first side from popup_preferred_sides, for which the popup with
// a `popup_preferred_size` fits on the side of the `element_bounds` in
// the `content_area_bounds` taking the arrow length into account.
// If neither side bits, the function returns kBottom.
// `anchor_type` is used to define whether the `element_bounds` width
// is taken into account to decide if vertical positioning is allowed.
// In the case of `PopupAnchorType::kCaret`, this width
// check does not apply. Since carets are narrow by design.
views::BubbleArrowSide GetOptimalArrowSide(
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    const gfx::Size& popup_preferred_size,
    base::span<const views::BubbleArrowSide> popup_preferred_sides,
    PopupAnchorType anchor_type);

// Determines the optimal position of a popup with `popup_preferred_size` next
// to an UI element with `element_bounds`. The arrow pointer dimensions are
// not taken into account if it is present. `content_area_bounds` are the
// boundaries of the view port, `right_to_left` indicates if the website uses
// text written from right to left. `scrollbar_width` is the width of a scroll
// bar and `maximum_offset_to_center` is the maximum number of pixels the popup
// can be moved towards the center of `element_bounds` and
// `maximum_width_percentage_to_center` is the maxmum percentage of the
// element's width for the popup to move towards the center. `popup_bounds` is
// the current rect of the popup that is modified by this function. The
// function returns the arrow position that is used on the popup.
views::BubbleBorder::Arrow GetOptimalPopupPlacement(
    const gfx::Rect& content_area_bounds,
    const gfx::Rect& element_bounds,
    const gfx::Size& popup_preferred_size,
    bool right_to_left,
    int scrollbar_width,
    int maximum_pixel_offset_to_center,
    int maximum_width_percentage_to_center,
    gfx::Rect& popup_bounds,
    base::span<const views::BubbleArrowSide> popup_preferred_sides,
    PopupAnchorType anchor_type);

// Returns whether there is an open permissions prompt in `web_contents` with
// bounds that overlap `screen_bounds`.
bool BoundsOverlapWithOpenPermissionsPrompt(const gfx::Rect& screen_bounds,
                                            content::WebContents* web_contents);

// Returns whether there is picture-in-picture window with bounds that overlap
// `screen_bounds`. Assuming:
// 1. the pip window is shown in the active tab.
// 2. this function is called from the frame of the contents that show the
// autofill popup.
bool BoundsOverlapWithPictureInPictureWindow(const gfx::Rect& screen_bounds);

// Returns whether a popup may vertically exceed the bounds of `web_contents`.
// This is permitted for extension popups. Here we only enforce that the
// autofill popup is at least attached to the extension popup (or overlaps the
// extension popup) and stays within the bounds of the browser window.
bool PopupMayExceedContentAreaBounds(content::WebContents* web_contents);

// Returns whether the suggestion with this `type` can have child
// suggestions.
bool IsExpandableSuggestionType(SuggestionType type);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_UTILS_H_
