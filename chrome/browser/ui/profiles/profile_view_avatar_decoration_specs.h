// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_AVATAR_DECORATION_SPECS_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_AVATAR_DECORATION_SPECS_H_

#include <array>

#include "ui/color/color_id.h"

/* Placeholder public specs for the linear gradient avatar ring. */

// Starting color ID of the linear gradient ring (public fallback: neutral dark
// grey).
inline constexpr ui::ColorId kAvatarRingGradientStartColorId =
    ui::kColorRefNeutral20;

// Ending color ID of the linear gradient ring (public fallback: neutral light
// grey).
inline constexpr ui::ColorId kAvatarRingGradientEndColorId =
    ui::kColorRefNeutral80;

// Relative color stops (0.0 to 1.0) along the linear gradient vector.
inline constexpr std::array<float, 4> kAvatarRingGradientPositions = {
    0.0f, 0.33f, 0.66f, 1.0f};

// Normalized starting point (x, y) of the gradient vector relative to the
// bounding box.
inline constexpr std::array<float, 2> kAvatarRingGradientP1Normalized = {0.0f,
                                                                         1.0f};

// Normalized ending point (x, y) of the gradient vector relative to the
// bounding box.
inline constexpr std::array<float, 2> kAvatarRingGradientP2Normalized = {1.0f,
                                                                         0.0f};

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_AVATAR_DECORATION_SPECS_H_
