// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_VIEW_SIZE_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_VIEW_SIZE_CONSTANTS_H_

namespace ash {

// TODO(emaamari): Most of the constants in this file are taking from
// `ash/login/ui/login_password_view.cc`. Resolve the duplication of code by
// making `ash/login/ui/login_password_view.cc` use the constants in this file
// instead.

// Spacing between glyphs, used when password is in hidden state.
constexpr int kPasswordGlyphSpacing = 6;

// External padding on the submit button, used for the focus ring.
constexpr const int kBorderForFocusRingDp = 3;

// Spacing between the password row and the submit button.
constexpr int kSpacingBetweenPasswordRowAndSubmitButtonDp =
    8 - kBorderForFocusRingDp;

// Size (width/height) of the submit button.
constexpr int kSubmitButtonContentSizeDp = 32;

// Size (width/height) of the submit button, border included.
constexpr int kSubmitButtonSizeDp =
    kSubmitButtonContentSizeDp + 2 * kBorderForFocusRingDp;

// Left padding of the password view allowing the view to have its center
// aligned with the one of the user pod.
constexpr int kLeftPaddingPasswordView =
    kSubmitButtonSizeDp + kSpacingBetweenPasswordRowAndSubmitButtonDp;

// Width of the password row, placed at the center of the password view
// (which also contains the submit button).
constexpr int kPasswordRowWidthDp = 204 + kBorderForFocusRingDp;

// Total width of the password view (left margin + password row + spacing +
// submit button).
constexpr int kPasswordTotalWidthDp =
    kLeftPaddingPasswordView + kPasswordRowWidthDp + kSubmitButtonSizeDp +
    kSpacingBetweenPasswordRowAndSubmitButtonDp;

// Size (width/height) of the different icons belonging to the password row
// (the display password icon and the caps lock icon).
constexpr const int kIconSizeDp = 20;

// Delta between normal font and font of the typed text.
constexpr int kPasswordVisibleFontDeltaSize = 1;

// Spacing between the icons (caps lock, display password) and the
// borders of the password row. Note that if there is no icon, the padding will
// appear to be 8dp since the password textfield has a 2dp margin.
constexpr const int kInternalHorizontalPaddingPasswordRowDp = 6;

// Horizontal spacing between the end of the password textfield and the display
// password button. Note that the password textfield has a 2dp margin so the
// ending result will be 8dp.
constexpr const int kHorizontalSpacingBetweenIconsAndTextfieldDp = 6;

// Delta between normal font and font of glyphs.
constexpr int kPasswordHiddenFontDeltaSize = 12;

// Line-height of the password hidden font, used to limit the height of the
// cursor.
constexpr int kPasswordHiddenLineHeight = 20;

// The password textfield has an external margin because we want these specific
// visual results following in these different cases:
// icon-textfield-icon: 6dp - icon - 8dp - textfield - 8dp - icon - 6dp
// textfield-icon:      8dp - textfield - 8dp - icon - 6dp
// icon-textfield:      6dp - icon - 8dp - textfield - 8dp
// textfield:           8dp - textfield - 8dp
// This translates by having a 6dp spacing between children of the paassword
// row, having a 6dp padding for the password row and having a 2dp margin for
// the password textfield.
constexpr const int kPasswordTextfieldMarginDp = 2;

constexpr const int kPasswordRowCornerRadiusDp = 4;

constexpr const int kLoginPasswordRowRoundedRectRadius = 8;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_VIEW_SIZE_CONSTANTS_H_
