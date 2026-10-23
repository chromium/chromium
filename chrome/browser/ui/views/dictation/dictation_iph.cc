// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_iph.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/views/view.h"

namespace dictation {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationIphAnchorElementId);

namespace {

using user_education::HelpBubbleArrow;

// The arrow is chosen before the bubble is created, so its final size is not
// known yet. This is the bubble size from the UX spec.
constexpr gfx::Size kEstimatedBubbleSize(340, 184);

// A top or bottom arrow needs the visible part of the field to be at least
// this wide, so the arrow points at the field rather than past its edge. Same
// threshold as the autofill popup.
constexpr int kMinFieldWidthForVerticalArrow =
    3 * views::BubbleBorder::kVisibleArrowRadius;

}  // namespace

HelpBubbleArrow GetDictationIphArrowForBounds(const gfx::Rect& field_bounds,
                                              const gfx::Rect& visible_bounds,
                                              const gfx::Size& bubble_size,
                                              bool is_rtl) {
  // The bubble sits this far from the field: the gap plus the arrow itself.
  const int arrow_space = views::BubbleBorder::kVisibleArrowGap +
                          views::BubbleBorder::kVisibleArrowLength;
  const int needed_height = bubble_size.height() + arrow_space;
  const int needed_width = bubble_size.width() + arrow_space;

  const int space_above = field_bounds.y() - visible_bounds.y();
  const int space_below = visible_bounds.bottom() - field_bounds.bottom();
  const int space_right = visible_bounds.right() - field_bounds.right();
  const int space_left = field_bounds.x() - visible_bounds.x();

  const int visible_field_width =
      std::clamp(field_bounds.right(), visible_bounds.x(),
                 visible_bounds.right()) -
      std::clamp(field_bounds.x(), visible_bounds.x(), visible_bounds.right());
  const bool can_use_vertical_arrow =
      visible_field_width > kMinFieldWidthForVerticalArrow;

  // `views::BubbleDialogDelegate` horizontally mirrors the arrow in RTL. For
  // bubbles above or below the field that is what we want: the bubble stays
  // aligned with the start edge of the field. For bubbles to the right or left
  // of the field, pre-mirror the arrow so the bubble lands on the requested
  // physical side.
  //
  // Arrow naming: the arrow side is opposite the bubble side, e.g. a bubble
  // above the field has its arrow on its bottom edge (kBottomLeft).
  const HelpBubbleArrow arrow_from_above = HelpBubbleArrow::kBottomLeft;
  const HelpBubbleArrow arrow_from_below = HelpBubbleArrow::kTopLeft;
  const HelpBubbleArrow arrow_from_right =
      is_rtl ? HelpBubbleArrow::kRightTop : HelpBubbleArrow::kLeftTop;
  const HelpBubbleArrow arrow_from_left =
      is_rtl ? HelpBubbleArrow::kLeftTop : HelpBubbleArrow::kRightTop;

  // Priority: below, left, right, above.
  if (can_use_vertical_arrow && space_below >= needed_height) {
    return arrow_from_below;
  }
  if (space_left >= needed_width) {
    return arrow_from_left;
  }
  if (space_right >= needed_width) {
    return arrow_from_right;
  }
  if (can_use_vertical_arrow && space_above >= needed_height) {
    return arrow_from_above;
  }

  // Nothing fits, e.g. the field fills most of the page. Use whichever of
  // below/above has more space; the bubble may overlap the field.
  return space_above > space_below ? arrow_from_above : arrow_from_below;
}

HelpBubbleArrow GetDictationIphArrow(const ui::TrackedElement* anchor_element) {
  const auto* const anchor = anchor_element->AsA<views::ViewSubregionAnchor>();
  if (!anchor) {
    return HelpBubbleArrow::kTopLeft;
  }
  // The host is the `ContentsWebView`, so its bounds are the visible page area.
  const gfx::Rect visible_bounds = anchor->view().GetBoundsInScreen();
  return GetDictationIphArrowForBounds(anchor->GetScreenBounds(),
                                       visible_bounds, kEstimatedBubbleSize,
                                       base::i18n::IsRTL());
}

}  // namespace dictation
