// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"

#include <algorithm>
#include <optional>

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

using views::BubbleBorder;

namespace autofill {

namespace {

// The minimum number of pixels the popup should be distanced from the edge of
// the content area.
constexpr int kMinimalPopupDistanceToContentAreaEdge = 8;

// Returns true if the arrow is either located on top or on the bottom of the
// popup.
bool IsVerticalArrowSide(views::BubbleArrowSide side) {
  return side == views::BubbleArrowSide::kTop ||
         side == views::BubbleArrowSide::kBottom;
}

// Returns false if the element is not sufficiently wide to place an arrow.
// A vertical arrow points upwards or downwards and we require an input element
// to be at least 3x wider than the arrow for esthetic reasons.
// This must only be called for vertical arrows (pointing up or down).
bool IsElementSufficientlyVisibleForAVerticalArrow(
    const gfx::Rect& visible_content_area_bounds,
    const gfx::Rect& element_bounds,
    views::BubbleArrowSide side) {
  CHECK(IsVerticalArrowSide(side));

  int visible_width =
      std::clamp(element_bounds.right(), visible_content_area_bounds.x(),
                 visible_content_area_bounds.right()) -
      std::clamp(element_bounds.x(), visible_content_area_bounds.x(),
                 visible_content_area_bounds.right());

  return visible_width > 3 * BubbleBorder::kVisibleArrowRadius;
}

// Returns a BubbleBorder::Arrow that is suitable for the supplied |side| and
// text direction.
BubbleBorder::Arrow GetBubbleArrowForBubbleArrowSide(
    views::BubbleArrowSide side,
    bool right_to_left) {
  switch (side) {
    case views::BubbleArrowSide::kTop:
      return right_to_left ? BubbleBorder::Arrow::TOP_RIGHT
                           : BubbleBorder::Arrow::TOP_LEFT;

    case views::BubbleArrowSide::kBottom:
      return right_to_left ? BubbleBorder::Arrow::BOTTOM_RIGHT
                           : BubbleBorder::Arrow::BOTTOM_LEFT;

    case views::BubbleArrowSide::kLeft:
      return BubbleBorder::Arrow::LEFT_TOP;

    case views::BubbleArrowSide::kRight:
      return BubbleBorder::Arrow::RIGHT_TOP;
  }
}

// Returns the size of popup placed on the |side| of the |element_bounds| once
// the popup is expanded to its |popup_preferred_size| or the maximum size
// available on the |visible_content_area_bounds|.
gfx::Size GetExpandedPopupSize(const gfx::Rect& visible_content_area_bounds,
                               const gfx::Rect& element_bounds,
                               const gfx::Size& popup_preferred_size,
                               int scrollbar_width,
                               views::BubbleArrowSide side) {
  // Get the maximum available space for the popup
  int available_height = GetAvailableVerticalSpaceOnSideOfElement(
      visible_content_area_bounds, element_bounds, side);
  int available_width = GetAvailableHorizontalSpaceOnSideOfElement(
      visible_content_area_bounds, element_bounds, side);

  int height = std::min(available_height, popup_preferred_size.height());
  int width = std::min(
      available_width,
      popup_preferred_size.width() +
          (height < popup_preferred_size.height() ? scrollbar_width : 0));

  return {width, height};
}

// Returns whether there is a visible view with `view_id` that overlaps
// `screen_bounds`.
bool BoundsOverlapWithView(const gfx::Rect& screen_bounds,
                           BrowserView* browser_view,
                           ui::ElementIdentifier view_id) {
  auto* const view =
      BrowserElementsViews::From(browser_view->browser())->GetView(view_id);
  return view &&
         view->GetWidget()->GetWindowBoundsInScreen().Intersects(screen_bounds);
}

}  // namespace

void CalculatePopupYAndHeight(int popup_preferred_height,
                              const gfx::Rect& visible_content_area_bounds,
                              const gfx::Rect& element_bounds,
                              gfx::Rect* popup_bounds) {
  int top_growth_end =
      std::clamp(element_bounds.y(), visible_content_area_bounds.y(),
                 visible_content_area_bounds.bottom());
  int bottom_growth_start =
      std::clamp(element_bounds.bottom(), visible_content_area_bounds.y(),
                 visible_content_area_bounds.bottom());

  int top_available = top_growth_end - visible_content_area_bounds.y();
  int bottom_available =
      visible_content_area_bounds.bottom() - bottom_growth_start;

  popup_bounds->set_height(popup_preferred_height);
  popup_bounds->set_y(top_growth_end);

  int y_adjustment = (bottom_available >= popup_preferred_height ||
                      bottom_available >= top_available)
                         ? element_bounds.bottom()
                         : visible_content_area_bounds.y();
  popup_bounds->AdjustToFit(gfx::Rect(popup_bounds->x(), y_adjustment,
                                      popup_bounds->width(),
                                      visible_content_area_bounds.height()));
}

bool CanShowDropdownHere(int item_height,
                         const gfx::Rect& visible_content_area_bounds,
                         const gfx::Rect& element_bounds) {
  // Ensure that at least one row of the popup can be displayed within the
  // bounds of the content area so that the user notices the presence of the
  // popup.
  // It is sufficient to check if the element can be placed above or below the
  // element (and not on the left or right) because positioning on the sides
  // is only allowed if the "preferred size" of the popup fully fits to the
  // sides of the element. If that's not the case, the popup is placed above or
  // below the element as a fallback. This function ensures that the fallback
  // location provides enough space for at least one item in the content area
  // above the element.
  bool enough_space_for_one_item_in_content_area_above_element =
      element_bounds.y() - visible_content_area_bounds.y() >= item_height;
  bool element_top_is_within_content_area_bounds =
      element_bounds.y() >= visible_content_area_bounds.y() &&
      element_bounds.y() < visible_content_area_bounds.bottom();

  bool enough_space_for_one_item_in_content_area_below_element =
      visible_content_area_bounds.bottom() - element_bounds.bottom() >=
      item_height;
  bool element_bottom_is_within_content_area_bounds =
      element_bounds.bottom() > visible_content_area_bounds.y() &&
      element_bounds.bottom() <= visible_content_area_bounds.bottom();

  bool enough_space_vertically =
      (enough_space_for_one_item_in_content_area_above_element &&
       element_top_is_within_content_area_bounds) ||
      (enough_space_for_one_item_in_content_area_below_element &&
       element_bottom_is_within_content_area_bounds);

  // Ensure that the element is visible horizontally by
  // at least kMinHorizontalOverlapForPopup (100px).
  bool enough_space_horizontally =
      (element_bounds.right() - visible_content_area_bounds.x() >=
       kMinHorizontalOverlapForPopup) &&
      (visible_content_area_bounds.right() - element_bounds.x() >=
       kMinHorizontalOverlapForPopup);

  // Or, alternatively, the element fully fits horizontally into the screen.
  // It's useful for small elements, sized less then
  // kMinHorizontalOverlapForPopup. In that case, we would also want to show the
  // popup, since the whole element is visible.
  bool element_fully_fits_horizontally =
      visible_content_area_bounds.x() <= element_bounds.x() &&
      element_bounds.right() <= visible_content_area_bounds.right();

  enough_space_horizontally |= element_fully_fits_horizontally;

  return enough_space_vertically && enough_space_horizontally;
}

// Keep in sync with TryToCloseAllPrompts() from autofill_uitest.cc.
bool BoundsOverlapWithAnyOpenPrompt(const gfx::Rect& screen_bounds,
                                    content::WebContents* web_contents) {
  gfx::NativeWindow top_level_window = web_contents->GetTopLevelNativeWindow();
  // `top_level_window` can be `nullptr` if `web_contents` is not attached to
  // a window, e.g. in unit test runs.
  if (!top_level_window) {
    return false;
  }
  gfx::NativeView top_level_view =
      platform_util::GetViewForWindow(top_level_window);

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
  views::Widget::Widgets all_widgets =
      views::Widget::GetAllChildWidgets(top_level_view);
  return std::ranges::any_of(
      all_widgets, [&screen_bounds, web_contents_widget](views::Widget* w) {
        return w->IsDialogBox() && w->IsVisible() &&
               w->GetWindowBoundsInScreen().Intersects(screen_bounds) &&
               w != web_contents_widget;
      });
}

bool BoundsOverlapWithOpenPermissionsPrompt(
    const gfx::Rect& screen_bounds,
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return false;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return false;
  }

  return BoundsOverlapWithView(screen_bounds, browser_view,
                               PermissionPromptBubbleBaseView::kMainViewId) ||
         BoundsOverlapWithView(screen_bounds, browser_view,
                               EmbeddedPermissionPromptBaseView::kMainViewId);
}

bool BoundsOverlapWithPictureInPictureWindow(const gfx::Rect& screen_bounds) {
  std::optional<gfx::Rect> pip_window_bounds =
      PictureInPictureWindowManager::GetInstance()
          ->GetPictureInPictureWindowBounds();
  return pip_window_bounds && pip_window_bounds->Intersects(screen_bounds);
}

bool PopupMayExceedContentAreaBounds(content::WebContents* web_contents) {
  if (!web_contents) {  // May be null for tests.
    return false;
  }
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

int GetAvailableVerticalSpaceOnSideOfElement(
    const gfx::Rect& visible_content_area_bounds,
    const gfx::Rect& element_bounds,
    views::BubbleArrowSide side) {
  // Note that the side of the arrow is opposite to the side of the element the
  // bubble is located on.
  switch (side) {
    case views::BubbleArrowSide::kLeft:
    case views::BubbleArrowSide::kRight:
      // For a bubble that is either on the left of the right side of the
      // element, the height of the content area is the total available space.
      return visible_content_area_bounds.height();

    case views::BubbleArrowSide::kBottom:
      // If the bubble sits above the element, return the space between the
      // upper edge of the element and the content area.
      return element_bounds.y() - visible_content_area_bounds.y();

    case views::BubbleArrowSide::kTop:
      // If the bubble sits below the element, return the space between the
      // lower edge of the element and the content area.
      return visible_content_area_bounds.bottom() - element_bounds.bottom();
  }
}

int GetAvailableHorizontalSpaceOnSideOfElement(
    const gfx::Rect& visible_content_area_bounds,
    const gfx::Rect& element_bounds,
    views::BubbleArrowSide side) {
  // Note that the side of the arrow is opposite to the side of the element the
  // popup is located on.
  switch (side) {
    case views::BubbleArrowSide::kRight:
      return element_bounds.x() - visible_content_area_bounds.x() -
             kMinimalPopupDistanceToContentAreaEdge;

    case views::BubbleArrowSide::kLeft:
      return visible_content_area_bounds.right() - element_bounds.right() -
             kMinimalPopupDistanceToContentAreaEdge;

    case views::BubbleArrowSide::kTop:
    case views::BubbleArrowSide::kBottom:
      return visible_content_area_bounds.width() -
             2 * kMinimalPopupDistanceToContentAreaEdge;
  }
}

bool IsPopupPlaceableOnSideOfElement(
    const gfx::Rect& visible_content_area_bounds,
    const gfx::Rect& element_bounds,
    const gfx::Size& popup_preferred_size,
    int additional_spacing,
    views::BubbleArrowSide side) {
  switch (side) {
    case views::BubbleArrowSide::kLeft:
    case views::BubbleArrowSide::kRight:
      return popup_preferred_size.width() + additional_spacing <=
             GetAvailableHorizontalSpaceOnSideOfElement(
                 visible_content_area_bounds, element_bounds, side);

    case views::BubbleArrowSide::kTop:
    case views::BubbleArrowSide::kBottom:
      return popup_preferred_size.height() + additional_spacing <=
             GetAvailableVerticalSpaceOnSideOfElement(
                 visible_content_area_bounds, element_bounds, side);
  }
}

gfx::Rect IntersectWithDisplayBounds(const gfx::Rect& element_bounds) {
  std::optional<gfx::Rect> display_bounds = GetDisplayBounds(element_bounds);
  if (display_bounds == std::nullopt) {
    return element_bounds;
  }
  display_bounds->Intersect(element_bounds);
  return display_bounds.value();
}

std::optional<gfx::Rect> GetDisplayBounds(const gfx::Rect& element_bounds) {
  display::Screen* screen = display::Screen::Get();
  if (!screen) {
    return std::nullopt;
  }

  return screen->GetDisplayMatching(element_bounds).work_area();
}

// Returns the first side (in the order of `popup_preferred_sides`) where the
// popup can be rendered in the preferred size. If no such site exists, falls
// back to the top of bottom of the element, depending on where more space is
// available.
views::BubbleArrowSide GetOptimalArrowSide(
    const gfx::Rect& visible_content_area_bounds,
    const gfx::Rect& element_bounds,
    const gfx::Size& popup_preferred_size,
    base::span<const views::BubbleArrowSide> popup_preferred_sides,
    PopupAnchorType anchor_type) {
  // Probe for a side of the element on which the popup can be shown entirely.
  for (views::BubbleArrowSide possible_side : popup_preferred_sides) {
    // If the popup is anchored on the cursor position (kCaret), do not check
    // whether the bounds of the element are sufficiently large to place a
    // vertical arrow because a caret is just one pixel wide.
    const bool
        skip_element_bounds_sufficiently_visible_for_vertical_arrow_check =
            anchor_type == PopupAnchorType::kCaret;
    // Is the element wide enough to place an arrow on the side. For arrows
    // pointing left or right, this is always true. For arrows pointing up or
    // down, the element must be sufficiently wide to place an arrow.
    const bool vertical_size_requirements_sufficient =
        !IsVerticalArrowSide(possible_side) ||
        skip_element_bounds_sufficiently_visible_for_vertical_arrow_check ||
        IsElementSufficientlyVisibleForAVerticalArrow(
            visible_content_area_bounds, element_bounds, possible_side);
    if (IsPopupPlaceableOnSideOfElement(
            visible_content_area_bounds, element_bounds, popup_preferred_size,
            BubbleBorder::kVisibleArrowLength, possible_side) &&
        vertical_size_requirements_sufficient) {
      return possible_side;
    }
  }

  // As a fallback, render the popup on top of the element if there is more
  // space than below the element.
  if (element_bounds.y() - visible_content_area_bounds.y() >
      visible_content_area_bounds.bottom() - element_bounds.bottom()) {
    return views::BubbleArrowSide::kBottom;
  }

  return views::BubbleArrowSide::kTop;
}

BubbleBorder::Arrow GetOptimalPopupPlacement(
    const gfx::Rect& visible_content_area_bounds,
    const gfx::Rect& element_bounds,
    const gfx::Size& popup_preferred_size,
    bool right_to_left,
    int scrollbar_width,
    int maximum_pixel_offset_to_center,
    int maximum_width_percentage_to_center,
    gfx::Rect& popup_bounds,
    base::span<const views::BubbleArrowSide> popup_preferred_sides,
    PopupAnchorType anchor_type) {
  // Determine the best side of the element to put the popup and get a
  // corresponding arrow. In the best case, this side fully fits the popup at
  // the `popup_preferred_size`. Otherwise, the popup is rendered on top or
  // below the element, depending on where more space is available.
  views::BubbleArrowSide side = GetOptimalArrowSide(
      visible_content_area_bounds, element_bounds, popup_preferred_size,
      popup_preferred_sides, anchor_type);
  BubbleBorder::Arrow arrow =
      GetBubbleArrowForBubbleArrowSide(side, right_to_left);

  // Set the actual size of the popup.
  popup_bounds.set_size(
      GetExpandedPopupSize(visible_content_area_bounds, element_bounds,
                           popup_preferred_size, scrollbar_width, side));

  // Move the origin of the popup to the anchor position on the element
  // corresponding to |arrow|.
  //                   ------------------
  //  For TOP_LEFT    |      element     |
  //  anchor_point ->  ==============----
  //                  |              |
  //                  |    popup     |
  //                  |              |
  //                  |              |
  //                   --------------
  popup_bounds += views::GetContentBoundsOffsetToArrowAnchorPoint(
      popup_bounds, arrow,
      views::GetArrowAnchorPointFromAnchorRect(arrow, element_bounds));

  if (!IsVerticalArrowSide(side)) {
    // For a horizontal arrow, move the popup to the top if it leaves the
    // lower part of the screen. Note, that by default, the popup's top is
    // aligned with the field.
    // The popup top can never go above the content area since the popup size
    // computed to fit in the screen by GetExpandedPopupSize.
    popup_bounds.Offset(
        0, -1 * std::max(0, popup_bounds.bottom() -
                                visible_content_area_bounds.bottom()));
    return arrow;
  }

  // The horizontal offset is the minimum of a fixed number of pixels
  // |maximum_pixel_offset_to_center| and a percentage of the element width.
  int horizontal_offset_pixels = std::min(
      maximum_pixel_offset_to_center,
      maximum_width_percentage_to_center * element_bounds.width() / 100);

  // In addition, the offset is shifted by the distance of the popup's arrow to
  // the popup's edge. By this, the arrow of the popup is aligned with the
  // targeted pixel and not the edge of the popup.
  horizontal_offset_pixels -=
      (BubbleBorder::kVisibleArrowBuffer + BubbleBorder::kVisibleArrowRadius);

  // Give the offset a direction.
  int horizontal_offset = horizontal_offset_pixels * (right_to_left ? -1 : 1);

  // Move the popup bounds towards to center of the field.
  // Note that for |right_to_left|, this will be a negative value.
  //              ------------------
  //             |      element     |
  //              ----------========-------
  //                       |               |
  //             |---------|    popup      |
  //   horizontal offset   |               |
  //                       |               |
  //                        ---------------
  popup_bounds.Offset(horizontal_offset, 0);

  // In case the popup the exceeds the right edge of the view port, move it
  // back until it completely fits.
  //              ------------------   |---| shift back
  //             |      element     |  |
  //              ----------========---+---
  //                       |           |   |
  //                       |    popup  |   |
  //                       |           |   |
  //                       |           |   |
  //                        -----------+---
  //                                   |
  //                          content_area.right()
  popup_bounds.Offset(
      std::min(0, visible_content_area_bounds.right() - popup_bounds.right() -
                      kMinimalPopupDistanceToContentAreaEdge),
      0);

  // Analogously, make move the popup to the right if it exceeds the left edge
  // of the content area.
  popup_bounds.Offset(
      std::max(0, visible_content_area_bounds.x() - popup_bounds.x() +
                      kMinimalPopupDistanceToContentAreaEdge),
      0);

  return arrow;
}

bool IsExpandableSuggestionType(SuggestionType type) {
  switch (type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kPasswordEntry:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kFillPassword:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kOneTimePasswordEntry:
      return false;
  }
}

}  // namespace autofill
