// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_CONSTANTS_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

constexpr gfx::Size kMultitaskButtonLandscapeSize(108, 72);
constexpr gfx::Size kMultitaskButtonPortraitSize(72, 108);
constexpr gfx::Size kFloatPatternSize(32, 44);
constexpr int kMultitaskBaseButtonBorderRadius = 7;
constexpr int kButtonBorderSize = 1;
constexpr int kButtonCornerRadius = 4;
constexpr float kButtonPadding = 4.f;
constexpr SkAlpha kMultitaskDefaultButtonOpacity = SK_AlphaOPAQUE * 0.21;
constexpr SkAlpha kMultitaskHoverButtonOpacity = SK_AlphaOPAQUE * 0.40;
constexpr SkAlpha kMultitaskHoverBackgroundOpacity = SK_AlphaOPAQUE * 0.12;
constexpr SkAlpha kMultitaskDisabledButtonOpacity = SK_AlphaOPAQUE * 0.15;

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_CONSTANTS_H_
