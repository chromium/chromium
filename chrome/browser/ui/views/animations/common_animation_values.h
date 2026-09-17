// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ANIMATIONS_COMMON_ANIMATION_VALUES_H_
#define CHROME_BROWSER_UI_VIEWS_ANIMATIONS_COMMON_ANIMATION_VALUES_H_

#include "ui/gfx/animation/tween.h"

namespace browser_animations {

// Common parameters for panels why fly out over the browser's content area
// (vertical tab strip expand-on-hover, organizer panel, etc.)
inline constexpr int kFlyoutShowMs = 250;
inline constexpr int kFlyoutHideMs = 200;
inline constexpr gfx::Tween::Type kFlyoutTween =
    gfx::Tween::EASE_IN_OUT_EMPHASIZED;
inline constexpr int kFlyoutFadeMs = 100;
inline constexpr gfx::Tween::Type kFlyoutFadeInTween = gfx::Tween::EASE_OUT;
inline constexpr gfx::Tween::Type kFlyoutFadeOutTween =
    gfx::Tween::FAST_OUT_LINEAR_IN;

}  // namespace browser_animations

#endif  // CHROME_BROWSER_UI_VIEWS_ANIMATIONS_COMMON_ANIMATION_VALUES_H_
