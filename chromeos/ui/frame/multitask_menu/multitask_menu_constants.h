// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_CONSTANTS_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_CONSTANTS_H_

#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

constexpr gfx::Size kMultitaskButtonLandscapeSize(108, 72);
constexpr gfx::Size kMultitaskButtonPortraitSize(72, 108);
constexpr gfx::Insets kMultitaskBaseButtonMargin(4);
constexpr int kMultitaskBaseButtonBorderRadius = 7;
constexpr int kButtonBorderSize = 1;
constexpr int kButtonCornerRadius = 4;

// Default color for button pattern and border in default state.
// TODO(shidi): Will replace these once color provider is integrated.
constexpr SkColor kMultitaskButtonDefaultColor =
    SkColorSetA(gfx::kGoogleGrey600, SK_AlphaOPAQUE * 0.58);

// When a button is hovered, the color changed to
// `kMultitaskButtonPrimaryHoverColor`, and the other button on the same
// MultitaskButtonView changed to `kMultitaskButtonViewHoverColor`.
constexpr SkColor kMultitaskButtonPrimaryHoverColor =
    SkColorSetA(gfx::kGoogleBlue600, SK_AlphaOPAQUE);
constexpr SkColor kMultitaskButtonViewHoverColor =
    SkColorSetA(gfx::kGoogleBlue600, SK_AlphaOPAQUE * 0.12);

// The pattern color of both primary and secondary buttons when it's disabled.
constexpr SkColor kMultitaskButtonDisabledColor =
    SkColorSetA(gfx::kGoogleGrey600, SK_AlphaOPAQUE * 0.28);

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_CONSTANTS_H_
