// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BADGE_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BADGE_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/views/style/typography.h"

namespace autofill {

// Text style for round corner badges in popup rows.
inline constexpr auto kPopupBadgeTextStyle =
    views::style::TextStyle::STYLE_BODY_5;

// Border insets for round corner badges in popup rows.
inline constexpr gfx::Insets kPopupBadgeBorderInsets =
    gfx::Insets::TLBR(/*top=*/2,
                      /*left=*/8,
                      /*bottom=*/2,
                      /*right=*/8);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BADGE_CONSTANTS_H_
