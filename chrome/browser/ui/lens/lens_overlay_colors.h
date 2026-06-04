// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace lens {

// clang-format off
// Colors pulled from
// https://www.figma.com/design/MkL8eSQvcZ9hlO3jcx47Jt/Chromnient-I%2FO-Specs
// Fallback colors:
inline constexpr SkColor kColorFallbackPrimary = SkColorSetRGB(0x18, 0x1C, 0x22);
inline constexpr SkColor kColorFallbackShaderLayer1 = SkColorSetRGB(0x5B, 0x5E, 0x66);
inline constexpr SkColor kColorFallbackShaderLayer2 = SkColorSetRGB(0x8E, 0x91, 0x99);
inline constexpr SkColor kColorFallbackShaderLayer3 = SkColorSetRGB(0xA6, 0xC8, 0xFF);
inline constexpr SkColor kColorFallbackShaderLayer4 = SkColorSetRGB(0xEE, 0xF0, 0xF9);
inline constexpr SkColor kColorFallbackShaderLayer5 = SkColorSetRGB(0xA8, 0xAB, 0xB3);
inline constexpr SkColor kColorFallbackScrim = SkColorSetRGB(0x18, 0x1C, 0x22);
inline constexpr SkColor kColorFallbackSurfaceContainerHighestLight = SkColorSetRGB(0xE0, 0xE2, 0xEB);
inline constexpr SkColor kColorFallbackSurfaceContainerHighestDark = SkColorSetRGB(0x43, 0x47, 0x4E);
inline constexpr SkColor kColorFallbackSelectionElement = SkColorSetRGB(0xEE, 0xF0, 0xF9);
inline constexpr SkColor kColorLineSelectionGradient1 = SkColorSetRGB(0x8F, 0xAA, 0xEB);
inline constexpr SkColor kColorLineSelectionGradient2 = SkColorSetRGB(0xB7, 0xB8, 0xEF);
inline constexpr SkColor kColorLineSelectionGradient3 = SkColorSetRGB(0x98, 0xBE, 0xED);
// clang-format on


}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_
