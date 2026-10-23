// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_H_

#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace ui {
class TrackedElement;
}  // namespace ui

namespace dictation {

// Identifies the anchor that tracks the focused text field. The dictation IPH
// is registered against this identifier.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDictationIphAnchorElementId);

// Returns the arrow to use for a help bubble of `bubble_size` placed next to
// `field_bounds` so that the bubble fits inside `visible_bounds`. All rects are
// in the same coordinate space (screen coordinates in production).
//
// Sides are tried in priority order: below, left, right, above. If the bubble
// does not fit on any side (e.g. a very large textarea), it falls back to
// whichever of below/above has more space.
//
// The returned arrow already accounts for `views::BubbleDialogDelegate`
// horizontally mirroring arrows in RTL, so the bubble ends up on the intended
// physical side of the field. Above/below bubbles align with the start edge of
// the field.
user_education::HelpBubbleArrow GetDictationIphArrowForBounds(
    const gfx::Rect& field_bounds,
    const gfx::Rect& visible_bounds,
    const gfx::Size& bubble_size,
    bool is_rtl);

// `FeaturePromoSpecification::HelpBubbleArrowCallback` for the dictation IPH.
// Computes the arrow from the anchor's current bounds and the bounds of the
// web contents view that hosts it.
user_education::HelpBubbleArrow GetDictationIphArrow(
    const ui::TrackedElement* anchor_element);

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_H_
