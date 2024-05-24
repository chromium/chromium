// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_

#include "base/containers/fixed_flat_map.h"
#include "third_party/skia/include/core/SkColor.h"

namespace lens {

// Colors pulled from
// https://www.figma.com/design/MkL8eSQvcZ9hlO3jcx47Jt/Chromnient-I%2FO-Specs

// clang-format off
// Palette primary colors:
inline constexpr SkColor kPaletteFallbackPrimaryColor = SK_ColorTRANSPARENT;
inline constexpr SkColor kPaletteGrapePrimaryColor = SkColorSetRGB(0x60, 0x18, 0xD6);
inline constexpr SkColor kPaletteCandyPrimaryColorPrimaryColor = SkColorSetRGB(0x95, 0x00, 0x84);
inline constexpr SkColor kPaletteGumPrimaryColor = SkColorSetRGB(0xA2, 0x00, 0x3B);
inline constexpr SkColor kPaletteTangerinePrimaryColor = SkColorSetRGB(0x8F, 0x31, 0x00);
inline constexpr SkColor kPaletteSchoolbusPrimaryColor = SkColorSetRGB(0x50, 0x42, 0x2B);
inline constexpr SkColor kPaletteCactusPrimaryColor = SkColorSetRGB(0x18, 0x5D, 0x00);
inline constexpr SkColor kPaletteTurquoisePrimaryColor = SkColorSetRGB(0x00, 0x5A, 0x5C);
inline constexpr SkColor kPaletteTomatoPrimaryColor = SkColorSetRGB(0xA3, 0x06, 0x21);
inline constexpr SkColor kPaletteCinnamonPrimaryColor = SkColorSetRGB(0x8E, 0x4E, 0x00);
inline constexpr SkColor kPaletteLemonPrimaryColor = SkColorSetRGB(0x6D, 0x5E, 0x00);
inline constexpr SkColor kPaletteLimePrimaryColor = SkColorSetRGB(0x56, 0x65, 0x00);
inline constexpr SkColor kPaletteEvergreenPrimaryColor = SkColorSetRGB(0x00, 0x6D, 0x42);
inline constexpr SkColor kPaletteMintPrimaryColor = SkColorSetRGB(0x00, 0x6B, 0x5B);
inline constexpr SkColor kPaletteIcePrimaryColor = SkColorSetRGB(0x00, 0x67, 0x7D);
inline constexpr SkColor kPaletteGlacierPrimaryColor = SkColorSetRGB(0x00, 0x65, 0x90);
inline constexpr SkColor kPaletteSapphirePrimaryColor = SkColorSetRGB(0x0A, 0x2B, 0xCE);
inline constexpr SkColor kPaletteLavenderPrimaryColor = SkColorSetRGB(0x74, 0x00, 0x9F);
// clang-format on

enum class PaletteId {
  kFallback,
  kGrape,
  kCandy,
  kGum,
  kTangerine,
  kSchoolbus,
  kCactus,
  kTurquoise,
  kTomato,
  kCinnamon,
  kLemon,
  kLime,
  kEvergreen,
  kMint,
  kIce,
  kGlacier,
  kSapphire,
  kLavender,
};

inline constexpr auto kPalettes = base::MakeFixedFlatMap<SkColor, PaletteId>({
    {kPaletteGrapePrimaryColor, PaletteId::kGrape},
    {kPaletteCandyPrimaryColorPrimaryColor, PaletteId::kCandy},
    {kPaletteGumPrimaryColor, PaletteId::kGum},
    {kPaletteTangerinePrimaryColor, PaletteId::kTangerine},
    {kPaletteSchoolbusPrimaryColor, PaletteId::kSchoolbus},
    {kPaletteCactusPrimaryColor, PaletteId::kCactus},
    {kPaletteTurquoisePrimaryColor, PaletteId::kTurquoise},
    {kPaletteTomatoPrimaryColor, PaletteId::kTomato},
    {kPaletteCinnamonPrimaryColor, PaletteId::kCinnamon},
    {kPaletteLemonPrimaryColor, PaletteId::kLemon},
    {kPaletteLimePrimaryColor, PaletteId::kLime},
    {kPaletteEvergreenPrimaryColor, PaletteId::kEvergreen},
    {kPaletteMintPrimaryColor, PaletteId::kMint},
    {kPaletteIcePrimaryColor, PaletteId::kIce},
    {kPaletteGlacierPrimaryColor, PaletteId::kGlacier},
    {kPaletteSapphirePrimaryColor, PaletteId::kSapphire},
    {kPaletteLavenderPrimaryColor, PaletteId::kLavender},
});

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_
