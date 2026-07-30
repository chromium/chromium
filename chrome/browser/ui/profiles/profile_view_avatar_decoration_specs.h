// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_AVATAR_DECORATION_SPECS_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_AVATAR_DECORATION_SPECS_H_

#include <array>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/profiles/avatar_badge_types.h"
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

#define HAS_AVATAR_BADGE_SPECS

/* Placeholder public specs for the avatar badge. */

// Background color ID of the avatar badge.
inline constexpr ui::ColorId kAvatarBadgeBackgroundColorId =
    ui::kColorRefNeutral10;

/* Placeholder public wave specs.
 *
 * Connection & Dimension Specification:
 * - `kAvatarBadgeWaveOps` [dimension: 5]: Ordered sequence of SVG path
 * operation opcodes: AvatarBadgePathOp::kMoveTo  (consumes 1 point  {x, y} from
 * `kAvatarBadgeWavePoints`) AvatarBadgePathOp::kCubicTo (consumes 3 points {p0,
 * p1, p2} from `kAvatarBadgeWavePoints`) AvatarBadgePathOp::kLineTo  (consumes
 * 1 point  {x, y} from `kAvatarBadgeWavePoints`) AvatarBadgePathOp::kClose
 * (consumes 0 points)
 * - `kAvatarBadgeWavePoints` [dimension: 6]: Sequential array of 2D coordinates
 * ({x, y}).
 *
 * Dimension Calculation:
 * - 1x MoveTo  => 1 * 1 = 1 point
 * - 1x CubicTo => 1 * 3 = 3 points
 * - 2x LineTo  => 2 * 1 = 2 points
 * - 1x Close   => 1 * 0 = 0 points
 * - Total Points = 1 + 3 + 2 + 0 = 6 points.
 */
inline constexpr std::array<AvatarBadgePathOp, 5> kAvatarBadgeWaveOps = {
    AvatarBadgePathOp::kMoveTo, AvatarBadgePathOp::kCubicTo,
    AvatarBadgePathOp::kLineTo, AvatarBadgePathOp::kLineTo,
    AvatarBadgePathOp::kClose};

inline constexpr std::array<std::array<float, 2>, 6> kAvatarBadgeWavePoints = {
    {{{-5.0f, 12.0f}},
     {{5.0f, 5.0f}},
     {{20.0f, 20.0f}},
     {{42.0f, 10.0f}},
     {{42.0f, 25.0f}},
     {{-5.0f, 25.0f}}}};

/* Fallback primitive badge constants for unbranded builds. */
inline constexpr float kAvatarBadgeUnscaledWidth = 37.0f;
inline constexpr float kAvatarBadgeUnscaledHeight = 22.0f;
inline constexpr float kAvatarBadgeShadowDx = 0.5f;
inline constexpr float kAvatarBadgeShadowDy = 1.0f;
inline constexpr float kAvatarBadgeShadowBlur = 3.0f;
inline constexpr float kAvatarBadgeShadowInversePadding = 20.0f;
inline constexpr ui::ColorId kAvatarBadgeShadowColorId = ui::kColorRefNeutral90;
inline constexpr float kAvatarBadgeWaveBlurFactor = 2.0f;

inline constexpr std::array<ui::ColorId, 2> kAvatarBadgeBaseColorIds = {
    kAvatarRingGradientStartColorId, kAvatarRingGradientEndColorId};
inline constexpr std::array<float, 2> kAvatarBadgeBaseOffsets = {0.0f, 1.0f};

inline constexpr std::array<ui::ColorId, 2> kAvatarBadgeOverlayColorIds = {
    kAvatarRingGradientStartColorId, kAvatarRingGradientEndColorId};
inline constexpr std::array<float, 2> kAvatarBadgeOverlayOffsets = {0.0f, 1.0f};

inline constexpr std::array<float, 2> kAvatarBadgeBaseStart = {0.0f, 0.0f};
inline constexpr std::array<float, 2> kAvatarBadgeBaseEnd = {37.0f, 22.0f};
inline constexpr std::array<float, 2> kAvatarBadgeOverlayStart = {0.0f, 22.0f};
inline constexpr std::array<float, 2> kAvatarBadgeOverlayEnd = {37.0f, 0.0f};

// Badge label string views for subscription tiers.
inline constexpr std::u16string_view kAvatarBadgeLabelTier1 = u"tier 1";
inline constexpr std::u16string_view kAvatarBadgeLabelTier2 = u"tier 2";
inline constexpr std::u16string_view kAvatarBadgeLabelTier3 = u"tier 3";

inline constexpr std::u16string_view kAvatarFullMembershipTier1 =
    u"Google Subscription tier 1";
inline constexpr std::u16string_view kAvatarFullMembershipTier2 =
    u"Google Subscription tier 2";
inline constexpr std::u16string_view kAvatarFullMembershipTier3 =
    u"Google Subscription tier 3";

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_AVATAR_DECORATION_SPECS_H_
