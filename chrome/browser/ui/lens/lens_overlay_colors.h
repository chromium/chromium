// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_

#include "base/containers/fixed_flat_map.h"
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
// Grape colors:
inline constexpr SkColor kColorGrapePrimary = SkColorSetRGB(0x60, 0x18, 0xD6);
// TODO(b/341968225): Add the remaining colors.
// Candy colors:
inline constexpr SkColor kColorCandyPrimary = SkColorSetRGB(0x95, 0x00, 0x84);
// TODO(b/341968225): Add the remaining colors.
// Gum colors:
inline constexpr SkColor kColorGumPrimary = SkColorSetRGB(0xA2, 0x00, 0x3B);
// TODO(b/341968225): Add the remaining colors.
// Tangerine colors:
inline constexpr SkColor kColorTangerinePrimary = SkColorSetRGB(0x8F, 0x31, 0x00);
// TODO(b/341968225): Add the remaining colors.
// Schoolbus colors:
inline constexpr SkColor kColorSchoolbusPrimary = SkColorSetRGB(0x50, 0x42, 0x2B);
// TODO(b/341968225): Add the remaining colors.
// Cactus colors:
inline constexpr SkColor kColorCactusPrimary = SkColorSetRGB(0x18, 0x5D, 0x00);
// TODO(b/341968225): Add the remaining colors.
// Turquoise colors:
inline constexpr SkColor kColorTurquoisePrimary = SkColorSetRGB(0x00, 0x5A, 0x5C);
// TODO(b/341968225): Add the remaining colors.
// Tomato colors:
inline constexpr SkColor kColorTomatoPrimary = SkColorSetRGB(0xA3, 0x06, 0x21);
// TODO(b/341968225): Add the remaining colors.
// Cinnamon colors:
inline constexpr SkColor kColorCinnamonPrimary = SkColorSetRGB(0x8E, 0x4E, 0x00);
// TODO(b/341968225): Add the remaining colors.
// Lemon colors:
inline constexpr SkColor kColorLemonPrimary = SkColorSetRGB(0x6D, 0x5E, 0x00);
// TODO(b/341968225): Add the remaining colors.
// Lime colors:
inline constexpr SkColor kColorLimePrimary = SkColorSetRGB(0x56, 0x65, 0x00);
// TODO(b/341968225): Add the remaining colors.
// Evergreen colors:
inline constexpr SkColor kColorEvergreenPrimary = SkColorSetRGB(0x00, 0x6D, 0x42);
// TODO(b/341968225): Add the remaining colors.
// Mint colors:
inline constexpr SkColor kColorMintPrimary = SkColorSetRGB(0x00, 0x6B, 0x5B);
// TODO(b/341968225): Add the remaining colors.
// Ice colors:
inline constexpr SkColor kColorIcePrimary = SkColorSetRGB(0x00, 0x67, 0x7D);
// TODO(b/341968225): Add the remaining colors.
// Glacier colors:
inline constexpr SkColor kColorGlacierPrimary = SkColorSetRGB(0x00, 0x65, 0x90);
// TODO(b/341968225): Add the remaining colors.
// Sapphire colors:
inline constexpr SkColor kColorSapphirePrimary = SkColorSetRGB(0x0A, 0x2B, 0xCE);
// TODO(b/341968225): Add the remaining colors.
// Lavender colors:
inline constexpr SkColor kColorLavenderPrimary = SkColorSetRGB(0x74, 0x00, 0x9F);
// TODO(b/341968225): Add the remaining colors.
// clang-format on

// Enumerates palettes.
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

// A look-up table mapping representative color of the palettes to the
// respective palette identifier. Used to look up palette identifier given a
// best matching representative color selected by the palette resolution logic.
inline constexpr auto kPalettes = base::MakeFixedFlatMap<SkColor, PaletteId>({
    {kColorGrapePrimary, PaletteId::kGrape},
    {kColorCandyPrimary, PaletteId::kCandy},
    {kColorGumPrimary, PaletteId::kGum},
    {kColorTangerinePrimary, PaletteId::kTangerine},
    {kColorSchoolbusPrimary, PaletteId::kSchoolbus},
    {kColorCactusPrimary, PaletteId::kCactus},
    {kColorTurquoisePrimary, PaletteId::kTurquoise},
    {kColorTomatoPrimary, PaletteId::kTomato},
    {kColorCinnamonPrimary, PaletteId::kCinnamon},
    {kColorLemonPrimary, PaletteId::kLemon},
    {kColorLimePrimary, PaletteId::kLime},
    {kColorEvergreenPrimary, PaletteId::kEvergreen},
    {kColorMintPrimary, PaletteId::kMint},
    {kColorIcePrimary, PaletteId::kIce},
    {kColorGlacierPrimary, PaletteId::kGlacier},
    {kColorSapphirePrimary, PaletteId::kSapphire},
    {kColorLavenderPrimary, PaletteId::kLavender},
});

// Enumerates the colors within a palette.
enum ColorId : size_t {
  kPrimary,
  kShaderLayer1,
  kShaderLayer2,
  kShaderLayer3,
  kShaderLayer4,
  kShaderLayer5,
  kScrim,
  kSurfaceContainerHighestLight,
  kSurfaceContainerHighestDark,
  kSelectionElement,
  kMax,
};

// TODO(b/341968225): Add rest of the mappings
// A look-up table mapping palette identifiers to the array of colors within the
// palette. The colors are indexed by the `ColorId` values.
inline constexpr auto kPaletteColors =
    base::MakeFixedFlatMap<PaletteId, std::array<SkColor, ColorId::kMax>>(
        {{PaletteId::kFallback,
          {
              kColorFallbackPrimary,
              kColorFallbackShaderLayer1,
              kColorFallbackShaderLayer2,
              kColorFallbackShaderLayer3,
              kColorFallbackShaderLayer4,
              kColorFallbackShaderLayer5,
              kColorFallbackScrim,
              kColorFallbackSurfaceContainerHighestLight,
              kColorFallbackSurfaceContainerHighestDark,
              kColorFallbackSelectionElement,
          }}});

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_
