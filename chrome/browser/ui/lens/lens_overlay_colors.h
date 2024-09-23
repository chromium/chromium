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
inline constexpr SkColor kColorGrapeShaderLayer1 = SkColorSetRGB(0x81, 0x69, 0xBB);
inline constexpr SkColor kColorGrapeShaderLayer2 = SkColorSetRGB(0x9B, 0x83, 0xD7);
inline constexpr SkColor kColorGrapeShaderLayer3 = SkColorSetRGB(0xC9, 0xFD, 0xD5);
inline constexpr SkColor kColorGrapeShaderLayer4 = SkColorSetRGB(0xE9, 0xDD, 0xFF);
inline constexpr SkColor kColorGrapeShaderLayer5 = SkColorSetRGB(0xD0, 0xBC, 0xFF);
inline constexpr SkColor kColorGrapeScrim = SkColorSetRGB(0x1E, 0x19, 0x28);
inline constexpr SkColor kColorGrapeSurfaceContainerHighestLight = SkColorSetRGB(0xE9, 0xDD, 0xFF);
inline constexpr SkColor kColorGrapeSurfaceContainerHighestDark = SkColorSetRGB(0x4C, 0x3F, 0x69);
inline constexpr SkColor kColorGrapeSelectionElement = SkColorSetRGB(0xF3, 0xEB, 0xFA);
// Candy colors:
inline constexpr SkColor kColorCandyPrimary = SkColorSetRGB(0x95, 0x00, 0x84);
inline constexpr SkColor kColorCandyShaderLayer1 = SkColorSetRGB(0xAB, 0x5B, 0x98);
inline constexpr SkColor kColorCandyShaderLayer2 = SkColorSetRGB(0xEA, 0x53, 0xCF);
inline constexpr SkColor kColorCandyShaderLayer3 = SkColorSetRGB(0xBB, 0xFD, 0xFC);
inline constexpr SkColor kColorCandyShaderLayer4 = SkColorSetRGB(0xFF, 0xD7, 0xF0);
inline constexpr SkColor kColorCandyShaderLayer5 = SkColorSetRGB(0xFF, 0x7A, 0xE3);
inline constexpr SkColor kColorCandyScrim = SkColorSetRGB(0x26, 0x17, 0x22);
inline constexpr SkColor kColorCandySurfaceContainerHighestLight = SkColorSetRGB(0xFF, 0xD7, 0xF0);
inline constexpr SkColor kColorCandySurfaceContainerHighestDark = SkColorSetRGB(0x66, 0x3D, 0x5F);
inline constexpr SkColor kColorCandySelectionElement = SkColorSetRGB(0xFD, 0xE8, 0xF3);
// Gum colors:
inline constexpr SkColor kColorGumPrimary = SkColorSetRGB(0xA2, 0x00, 0x3B);
inline constexpr SkColor kColorGumShaderLayer1 = SkColorSetRGB(0xDB, 0x35, 0x5F);
inline constexpr SkColor kColorGumShaderLayer2 = SkColorSetRGB(0xFE, 0x50, 0x77);
inline constexpr SkColor kColorGumShaderLayer3 = SkColorSetRGB(0xA3, 0xF2, 0xDE);
inline constexpr SkColor kColorGumShaderLayer4 = SkColorSetRGB(0xFF, 0xD9, 0xDD);
inline constexpr SkColor kColorGumShaderLayer5 = SkColorSetRGB(0xFF, 0x86, 0x9A);
inline constexpr SkColor kColorGumScrim = SkColorSetRGB(0x2A, 0x16, 0x19);
inline constexpr SkColor kColorGumSurfaceContainerHighestLight = SkColorSetRGB(0xFF, 0xD9, 0xDD);
inline constexpr SkColor kColorGumSurfaceContainerHighestDark = SkColorSetRGB(0x5F, 0x44, 0x47);
inline constexpr SkColor kColorGumSelectionElement = SkColorSetRGB(0xFF, 0xE9, 0xEA);
// Tangerine colors:
inline constexpr SkColor kColorTangerinePrimary = SkColorSetRGB(0x8F, 0x31, 0x00);
inline constexpr SkColor kColorTangerineShaderLayer1 = SkColorSetRGB(0xD0, 0x4B, 0x00);
inline constexpr SkColor kColorTangerineShaderLayer2 = SkColorSetRGB(0xF3, 0x63, 0x20);
inline constexpr SkColor kColorTangerineShaderLayer3 = SkColorSetRGB(0xA1, 0xEF, 0xFC);
inline constexpr SkColor kColorTangerineShaderLayer4 = SkColorSetRGB(0xFF, 0xDB, 0xCE);
inline constexpr SkColor kColorTangerineShaderLayer5 = SkColorSetRGB(0xFF, 0x8C, 0x5D);
inline constexpr SkColor kColorTangerineScrim = SkColorSetRGB(0x2A, 0x17, 0x10);
inline constexpr SkColor kColorTangerineSurfaceContainerHighestLight = SkColorSetRGB(0xFF, 0xDB, 0xCE);
inline constexpr SkColor kColorTangerineSurfaceContainerHighestDark = SkColorSetRGB(0x5D, 0x3F, 0x32);
inline constexpr SkColor kColorTangerineSelectionElement = SkColorSetRGB(0xFF, 0xED, 0xE7);
// Schoolbus colors:
inline constexpr SkColor kColorSchoolbusPrimary = SkColorSetRGB(0x50, 0x42, 0x2B);
inline constexpr SkColor kColorSchoolbusShaderLayer1 = SkColorSetRGB(0x9D, 0x6E, 0x00);
inline constexpr SkColor kColorSchoolbusShaderLayer2 = SkColorSetRGB(0xBB, 0x88, 0x22);
inline constexpr SkColor kColorSchoolbusShaderLayer3 = SkColorSetRGB(0xAC, 0xC7, 0xFF);
inline constexpr SkColor kColorSchoolbusShaderLayer4 = SkColorSetRGB(0xF0, 0xE0, 0xCB);
inline constexpr SkColor kColorSchoolbusShaderLayer5 = SkColorSetRGB(0xF7, 0xBD, 0x54);
inline constexpr SkColor kColorSchoolbusScrim = SkColorSetRGB(0x22, 0x1A, 0x0E);
inline constexpr SkColor kColorSchoolbusSurfaceContainerHighestLight = SkColorSetRGB(0xF0, 0xE0, 0xCB);
inline constexpr SkColor kColorSchoolbusSurfaceContainerHighestDark = SkColorSetRGB(0x50, 0x42, 0x2B);
inline constexpr SkColor kColorSchoolbusSelectionElement = SkColorSetRGB(0xFF, 0xEE, 0xD9);
// Cactus colors:
inline constexpr SkColor kColorCactusPrimary = SkColorSetRGB(0x18, 0x5D, 0x00);
inline constexpr SkColor kColorCactusShaderLayer1 = SkColorSetRGB(0x57, 0x82, 0x45);
inline constexpr SkColor kColorCactusShaderLayer2 = SkColorSetRGB(0x70, 0x9C, 0x5D);
inline constexpr SkColor kColorCactusShaderLayer3 = SkColorSetRGB(0xEC, 0xB3, 0xF4);
inline constexpr SkColor kColorCactusShaderLayer4 = SkColorSetRGB(0xDB, 0xE6, 0xD0);
inline constexpr SkColor kColorCactusShaderLayer5 = SkColorSetRGB(0xA4, 0xD4, 0x8E);
inline constexpr SkColor kColorCactusScrim = SkColorSetRGB(0x15, 0x1E, 0x10);
inline constexpr SkColor kColorCactusSurfaceContainerHighestLight = SkColorSetRGB(0xDB, 0xE6, 0xD0);
inline constexpr SkColor kColorCactusSurfaceContainerHighestDark = SkColorSetRGB(0x36, 0x4C, 0x2E);
inline constexpr SkColor kColorCactusSelectionElement = SkColorSetRGB(0xE9, 0xF5, 0xDE);
// Turquoise colors:
inline constexpr SkColor kColorTurquoisePrimary = SkColorSetRGB(0x00, 0x5A, 0x5C);
inline constexpr SkColor kColorTurquoiseShaderLayer1 = SkColorSetRGB(0x02, 0x84, 0x88);
inline constexpr SkColor kColorTurquoiseShaderLayer2 = SkColorSetRGB(0x36, 0x9F, 0xA3);
inline constexpr SkColor kColorTurquoiseShaderLayer3 = SkColorSetRGB(0xFF, 0xB2, 0xBD);
inline constexpr SkColor kColorTurquoiseShaderLayer4 = SkColorSetRGB(0xC5, 0xE9, 0xEB);
inline constexpr SkColor kColorTurquoiseShaderLayer5 = SkColorSetRGB(0x56, 0xBA, 0xBE);
inline constexpr SkColor kColorTurquoiseScrim = SkColorSetRGB(0x13, 0x1D, 0x1E);
inline constexpr SkColor kColorTurquoiseSurfaceContainerHighestLight = SkColorSetRGB(0xC5, 0xE9, 0xEB);
inline constexpr SkColor kColorTurquoiseSurfaceContainerHighestDark = SkColorSetRGB(0x33, 0x4F, 0x51);
inline constexpr SkColor kColorTurquoiseSelectionElement = SkColorSetRGB(0xE9, 0xF5, 0xDE);
// Tomato colors:
inline constexpr SkColor kColorTomatoPrimary = SkColorSetRGB(0xA3, 0x06, 0x21);
inline constexpr SkColor kColorTomatoShaderLayer1 = SkColorSetRGB(0xDB, 0x39, 0x43);
inline constexpr SkColor kColorTomatoShaderLayer2 = SkColorSetRGB(0xFF, 0x53, 0x5A);
inline constexpr SkColor kColorTomatoShaderLayer3 = SkColorSetRGB(0xA0, 0xF1, 0xE9);
inline constexpr SkColor kColorTomatoShaderLayer4 = SkColorSetRGB(0xFF, 0xDA, 0xD8);
inline constexpr SkColor kColorTomatoShaderLayer5 = SkColorSetRGB(0xFF, 0x88, 0x87);
inline constexpr SkColor kColorTomatoScrim = SkColorSetRGB(0x2A, 0x16, 0x15);
inline constexpr SkColor kColorTomatoSurfaceContainerHighestLight = SkColorSetRGB(0xFF, 0xDA, 0xD8);
inline constexpr SkColor kColorTomatoSurfaceContainerHighestDark = SkColorSetRGB(0x60, 0x45, 0x43);
inline constexpr SkColor kColorTomatoSelectionElement = SkColorSetRGB(0xFF, 0xED, 0xEB);
// Cinnamon colors:
inline constexpr SkColor kColorCinnamonPrimary = SkColorSetRGB(0x8E, 0x4E, 0x00);
inline constexpr SkColor kColorCinnamonShaderLayer1 = SkColorSetRGB(0xB2, 0x63, 0x00);
inline constexpr SkColor kColorCinnamonShaderLayer2 = SkColorSetRGB(0xD1, 0x7C, 0x22);
inline constexpr SkColor kColorCinnamonShaderLayer3 = SkColorSetRGB(0xBF, 0xE8, 0xFF);
inline constexpr SkColor kColorCinnamonShaderLayer4 = SkColorSetRGB(0xF7, 0xDE, 0xCC);
inline constexpr SkColor kColorCinnamonShaderLayer5 = SkColorSetRGB(0xF1, 0x96, 0x3B);
inline constexpr SkColor kColorCinnamonScrim = SkColorSetRGB(0x25, 0x19, 0x0E);
inline constexpr SkColor kColorCinnamonSurfaceContainerHighestLight = SkColorSetRGB(0xF7, 0xDE, 0xCC);
inline constexpr SkColor kColorCinnamonSurfaceContainerHighestDark = SkColorSetRGB(0x5A, 0x44, 0x30);
inline constexpr SkColor kColorCinnamonSelectionElement = SkColorSetRGB(0xFF, 0xEE, 0xE2);
// Lemon colors:
inline constexpr SkColor kColorLemonPrimary = SkColorSetRGB(0x6D, 0x5E, 0x00);
inline constexpr SkColor kColorLemonShaderLayer1 = SkColorSetRGB(0x89, 0x77, 0x00);
inline constexpr SkColor kColorLemonShaderLayer2 = SkColorSetRGB(0xA4, 0x91, 0x22);
inline constexpr SkColor kColorLemonShaderLayer3 = SkColorSetRGB(0xC0, 0xC1, 0xFF);
inline constexpr SkColor kColorLemonShaderLayer4 = SkColorSetRGB(0xEA, 0xE2, 0xCC);
inline constexpr SkColor kColorLemonShaderLayer5 = SkColorSetRGB(0xC0, 0xAB, 0x3C);
inline constexpr SkColor kColorLemonScrim = SkColorSetRGB(0x1F, 0x1C, 0x0E);
inline constexpr SkColor kColorLemonSurfaceContainerHighestLight = SkColorSetRGB(0xEA, 0xE2, 0xCC);
inline constexpr SkColor kColorLemonSurfaceContainerHighestDark = SkColorSetRGB(0x50, 0x4A, 0x2B);
inline constexpr SkColor kColorLemonSelectionElement = SkColorSetRGB(0xF8, 0xF0, 0xDA);
// Lime colors:
inline constexpr SkColor kColorLimePrimary = SkColorSetRGB(0x56, 0x65, 0x00);
inline constexpr SkColor kColorLimeShaderLayer1 = SkColorSetRGB(0x6D, 0x7F, 0x00);
inline constexpr SkColor kColorLimeShaderLayer2 = SkColorSetRGB(0x86, 0x99, 0x23);
inline constexpr SkColor kColorLimeShaderLayer3 = SkColorSetRGB(0xD6, 0xBA, 0xFF);
inline constexpr SkColor kColorLimeShaderLayer4 = SkColorSetRGB(0xE3, 0xE4, 0xCD);
inline constexpr SkColor kColorLimeShaderLayer5 = SkColorSetRGB(0xA0, 0xB5, 0x3D);
inline constexpr SkColor kColorLimeScrim = SkColorSetRGB(0x1A, 0x1D, 0x0F);
inline constexpr SkColor kColorLimeSurfaceContainerHighestLight = SkColorSetRGB(0xE3, 0xE4, 0xCD);
inline constexpr SkColor kColorLimeSurfaceContainerHighestDark = SkColorSetRGB(0x44, 0x49, 0x27);
inline constexpr SkColor kColorLimeSelectionElement = SkColorSetRGB(0xF1, 0xF2, 0xDB);
// Evergreen colors:
inline constexpr SkColor kColorEvergreenPrimary = SkColorSetRGB(0x00, 0x6D, 0x42);
inline constexpr SkColor kColorEvergreenShaderLayer1 = SkColorSetRGB(0x27, 0x89, 0x00);
inline constexpr SkColor kColorEvergreenShaderLayer2 = SkColorSetRGB(0x44, 0xA5, 0x24);
inline constexpr SkColor kColorEvergreenShaderLayer3 = SkColorSetRGB(0xF1, 0xB2, 0xEF);
inline constexpr SkColor kColorEvergreenShaderLayer4 = SkColorSetRGB(0xD9, 0xE6, 0xDA);
inline constexpr SkColor kColorEvergreenShaderLayer5 = SkColorSetRGB(0x5F, 0xC1, 0x3F);
inline constexpr SkColor kColorEvergreenScrim = SkColorSetRGB(0x15, 0x1E, 0x10);
inline constexpr SkColor kColorEvergreenSurfaceContainerHighestLight = SkColorSetRGB(0xF3, 0xEB, 0xFA);
inline constexpr SkColor kColorEvergreenSurfaceContainerHighestDark = SkColorSetRGB(0x32, 0x2F, 0x39);
inline constexpr SkColor kColorEvergreenSelectionElement = SkColorSetRGB(0xE7, 0xF4, 0xE8);
// Mint colors:
inline constexpr SkColor kColorMintPrimary = SkColorSetRGB(0x00, 0x6B, 0x5B);
inline constexpr SkColor kColorMintShaderLayer1 = SkColorSetRGB(0x01, 0x86, 0x73);
inline constexpr SkColor kColorMintShaderLayer2 = SkColorSetRGB(0x35, 0xA1, 0x8D);
inline constexpr SkColor kColorMintShaderLayer3 = SkColorSetRGB(0xFF, 0xAF, 0xD6);
inline constexpr SkColor kColorMintShaderLayer4 = SkColorSetRGB(0xD9, 0xE5, 0xE0);
inline constexpr SkColor kColorMintShaderLayer5 = SkColorSetRGB(0x55, 0xBC, 0xA7);
inline constexpr SkColor kColorMintScrim = SkColorSetRGB(0x13, 0x1E, 0x1B);
inline constexpr SkColor kColorMintSurfaceContainerHighestLight = SkColorSetRGB(0xD9, 0xE5, 0xE0);
inline constexpr SkColor kColorMintSurfaceContainerHighestDark = SkColorSetRGB(0x36, 0x4E, 0x49);
inline constexpr SkColor kColorMintSelectionElement = SkColorSetRGB(0xE7, 0xF4, 0xEE);
// Ice colors:
inline constexpr SkColor kColorIcePrimary = SkColorSetRGB(0x00, 0x67, 0x7D);
inline constexpr SkColor kColorIceShaderLayer1 = SkColorSetRGB(0x01, 0x82, 0x9C);
inline constexpr SkColor kColorIceShaderLayer2 = SkColorSetRGB(0x37, 0x9C, 0xB7);
inline constexpr SkColor kColorIceShaderLayer3 = SkColorSetRGB(0xFF, 0xB4, 0xA4);
inline constexpr SkColor kColorIceShaderLayer4 = SkColorSetRGB(0xC6, 0xE8, 0xF5);
inline constexpr SkColor kColorIceShaderLayer5 = SkColorSetRGB(0x57, 0xB7, 0xD3);
inline constexpr SkColor kColorIceScrim = SkColorSetRGB(0x13, 0x1D, 0x20);
inline constexpr SkColor kColorIceSurfaceContainerHighestLight = SkColorSetRGB(0xC6, 0xE8, 0xF5);
inline constexpr SkColor kColorIceSurfaceContainerHighestDark = SkColorSetRGB(0x30, 0x4C, 0x55);
inline constexpr SkColor kColorIceSelectionElement = SkColorSetRGB(0xE8, 0xF2, 0xF7);
// Glacier colors:
inline constexpr SkColor kColorGlacierPrimary = SkColorSetRGB(0x00, 0x65, 0x90);
inline constexpr SkColor kColorGlacierShaderLayer1 = SkColorSetRGB(0x00, 0x7B, 0xB4);
inline constexpr SkColor kColorGlacierShaderLayer2 = SkColorSetRGB(0x38, 0x99, 0xCF);
inline constexpr SkColor kColorGlacierShaderLayer3 = SkColorSetRGB(0xD7, 0xBA, 0xFF);
inline constexpr SkColor kColorGlacierShaderLayer4 = SkColorSetRGB(0xC8, 0xE6, 0xFF);
inline constexpr SkColor kColorGlacierShaderLayer5 = SkColorSetRGB(0x59, 0xB4, 0xEC);
inline constexpr SkColor kColorGlacierScrim = SkColorSetRGB(0x14, 0x1D, 0x23);
inline constexpr SkColor kColorGlacierSurfaceContainerHighestLight = SkColorSetRGB(0xC8, 0xE6, 0xFF);
inline constexpr SkColor kColorGlacierSurfaceContainerHighestDark = SkColorSetRGB(0x28, 0x47, 0x5D);
inline constexpr SkColor kColorGlacierSelectionElement = SkColorSetRGB(0xE9, 0xF2, 0xFB);
// Sapphire colors:
inline constexpr SkColor kColorSapphirePrimary = SkColorSetRGB(0x0A, 0x2B, 0xCE);
inline constexpr SkColor kColorSapphireShaderLayer1 = SkColorSetRGB(0x50, 0x66, 0xFF);
inline constexpr SkColor kColorSapphireShaderLayer2 = SkColorSetRGB(0x75, 0x86, 0xFF);
inline constexpr SkColor kColorSapphireShaderLayer3 = SkColorSetRGB(0xDA, 0xFB, 0xC1);
inline constexpr SkColor kColorSapphireShaderLayer4 = SkColorSetRGB(0xDF, 0xE0, 0xFF);
inline constexpr SkColor kColorSapphireShaderLayer5 = SkColorSetRGB(0x99, 0xA5, 0xFF);
inline constexpr SkColor kColorSapphireScrim = SkColorSetRGB(0x19, 0x1B, 0x28);
inline constexpr SkColor kColorSapphireSurfaceContainerHighestLight = SkColorSetRGB(0xDF, 0xE0, 0xFF);
inline constexpr SkColor kColorSapphireSurfaceContainerHighestDark = SkColorSetRGB(0x3F, 0x44, 0x69);
inline constexpr SkColor kColorSapphireSelectionElement = SkColorSetRGB(0xF0, 0xEF, 0xFF);
// Lavender colors:
inline constexpr SkColor kColorLavenderPrimary = SkColorSetRGB(0x74, 0x00, 0x9F);
inline constexpr SkColor kColorLavenderShaderLayer1 = SkColorSetRGB(0xB1, 0x40, 0xE2);
inline constexpr SkColor kColorLavenderShaderLayer2 = SkColorSetRGB(0xCE, 0x5D, 0xFE);
inline constexpr SkColor kColorLavenderShaderLayer3 = SkColorSetRGB(0xBE, 0xFE, 0xEA);
inline constexpr SkColor kColorLavenderShaderLayer4 = SkColorSetRGB(0xF8, 0xD8, 0xFF);
inline constexpr SkColor kColorLavenderShaderLayer5 = SkColorSetRGB(0xDD, 0x8A, 0xFF);
inline constexpr SkColor kColorLavenderScrim = SkColorSetRGB(0x22, 0x18, 0x26);
inline constexpr SkColor kColorLavenderSurfaceContainerHighestLight = SkColorSetRGB(0xF8, 0xD8, 0xFF);
inline constexpr SkColor kColorLavenderSurfaceContainerHighestDark = SkColorSetRGB(0x5A, 0x3D, 0x66);
inline constexpr SkColor kColorLavenderSelectionElement = SkColorSetRGB(0xFB, 0xEC, 0xFA);
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
          }},
         {PaletteId::kGrape,
          {
              kColorGrapePrimary,
              kColorGrapeShaderLayer1,
              kColorGrapeShaderLayer2,
              kColorGrapeShaderLayer3,
              kColorGrapeShaderLayer4,
              kColorGrapeShaderLayer5,
              kColorGrapeScrim,
              kColorGrapeSurfaceContainerHighestLight,
              kColorGrapeSurfaceContainerHighestDark,
              kColorGrapeSelectionElement,
          }},
         {PaletteId::kCandy,
          {
              kColorCandyPrimary,
              kColorCandyShaderLayer1,
              kColorCandyShaderLayer2,
              kColorCandyShaderLayer3,
              kColorCandyShaderLayer4,
              kColorCandyShaderLayer5,
              kColorCandyScrim,
              kColorCandySurfaceContainerHighestLight,
              kColorCandySurfaceContainerHighestDark,
              kColorCandySelectionElement,
          }},
         {PaletteId::kGum,
          {
              kColorGumPrimary,
              kColorGumShaderLayer1,
              kColorGumShaderLayer2,
              kColorGumShaderLayer3,
              kColorGumShaderLayer4,
              kColorGumShaderLayer5,
              kColorGumScrim,
              kColorGumSurfaceContainerHighestLight,
              kColorGumSurfaceContainerHighestDark,
              kColorGumSelectionElement,
          }},
         {PaletteId::kTangerine,
          {
              kColorTangerinePrimary,
              kColorTangerineShaderLayer1,
              kColorTangerineShaderLayer2,
              kColorTangerineShaderLayer3,
              kColorTangerineShaderLayer4,
              kColorTangerineShaderLayer5,
              kColorTangerineScrim,
              kColorTangerineSurfaceContainerHighestLight,
              kColorTangerineSurfaceContainerHighestDark,
              kColorTangerineSelectionElement,
          }},
         {PaletteId::kSchoolbus,
          {
              kColorSchoolbusPrimary,
              kColorSchoolbusShaderLayer1,
              kColorSchoolbusShaderLayer2,
              kColorSchoolbusShaderLayer3,
              kColorSchoolbusShaderLayer4,
              kColorSchoolbusShaderLayer5,
              kColorSchoolbusScrim,
              kColorSchoolbusSurfaceContainerHighestLight,
              kColorSchoolbusSurfaceContainerHighestDark,
              kColorSchoolbusSelectionElement,
          }},
         {PaletteId::kCactus,
          {
              kColorCactusPrimary,
              kColorCactusShaderLayer1,
              kColorCactusShaderLayer2,
              kColorCactusShaderLayer3,
              kColorCactusShaderLayer4,
              kColorCactusShaderLayer5,
              kColorCactusScrim,
              kColorCactusSurfaceContainerHighestLight,
              kColorCactusSurfaceContainerHighestDark,
              kColorCactusSelectionElement,
          }},
         {PaletteId::kTurquoise,
          {
              kColorTurquoisePrimary,
              kColorTurquoiseShaderLayer1,
              kColorTurquoiseShaderLayer2,
              kColorTurquoiseShaderLayer3,
              kColorTurquoiseShaderLayer4,
              kColorTurquoiseShaderLayer5,
              kColorTurquoiseScrim,
              kColorTurquoiseSurfaceContainerHighestLight,
              kColorTurquoiseSurfaceContainerHighestDark,
              kColorTurquoiseSelectionElement,
          }},
         {PaletteId::kTomato,
          {
              kColorTomatoPrimary,
              kColorTomatoShaderLayer1,
              kColorTomatoShaderLayer2,
              kColorTomatoShaderLayer3,
              kColorTomatoShaderLayer4,
              kColorTomatoShaderLayer5,
              kColorTomatoScrim,
              kColorTomatoSurfaceContainerHighestLight,
              kColorTomatoSurfaceContainerHighestDark,
              kColorTomatoSelectionElement,
          }},
         {PaletteId::kCinnamon,
          {
              kColorCinnamonPrimary,
              kColorCinnamonShaderLayer1,
              kColorCinnamonShaderLayer2,
              kColorCinnamonShaderLayer3,
              kColorCinnamonShaderLayer4,
              kColorCinnamonShaderLayer5,
              kColorCinnamonScrim,
              kColorCinnamonSurfaceContainerHighestLight,
              kColorCinnamonSurfaceContainerHighestDark,
              kColorCinnamonSelectionElement,
          }},
         {PaletteId::kLemon,
          {
              kColorLemonPrimary,
              kColorLemonShaderLayer1,
              kColorLemonShaderLayer2,
              kColorLemonShaderLayer3,
              kColorLemonShaderLayer4,
              kColorLemonShaderLayer5,
              kColorLemonScrim,
              kColorLemonSurfaceContainerHighestLight,
              kColorLemonSurfaceContainerHighestDark,
              kColorLemonSelectionElement,
          }},
         {PaletteId::kLime,
          {
              kColorLimePrimary,
              kColorLimeShaderLayer1,
              kColorLimeShaderLayer2,
              kColorLimeShaderLayer3,
              kColorLimeShaderLayer4,
              kColorLimeShaderLayer5,
              kColorLimeScrim,
              kColorLimeSurfaceContainerHighestLight,
              kColorLimeSurfaceContainerHighestDark,
              kColorLimeSelectionElement,
          }},
         {PaletteId::kEvergreen,
          {
              kColorEvergreenPrimary,
              kColorEvergreenShaderLayer1,
              kColorEvergreenShaderLayer2,
              kColorEvergreenShaderLayer3,
              kColorEvergreenShaderLayer4,
              kColorEvergreenShaderLayer5,
              kColorEvergreenScrim,
              kColorEvergreenSurfaceContainerHighestLight,
              kColorEvergreenSurfaceContainerHighestDark,
              kColorEvergreenSelectionElement,
          }},
         {PaletteId::kMint,
          {
              kColorMintPrimary,
              kColorMintShaderLayer1,
              kColorMintShaderLayer2,
              kColorMintShaderLayer3,
              kColorMintShaderLayer4,
              kColorMintShaderLayer5,
              kColorMintScrim,
              kColorMintSurfaceContainerHighestLight,
              kColorMintSurfaceContainerHighestDark,
              kColorMintSelectionElement,
          }},
         {PaletteId::kIce,
          {
              kColorIcePrimary,
              kColorIceShaderLayer1,
              kColorIceShaderLayer2,
              kColorIceShaderLayer3,
              kColorIceShaderLayer4,
              kColorIceShaderLayer5,
              kColorIceScrim,
              kColorIceSurfaceContainerHighestLight,
              kColorIceSurfaceContainerHighestDark,
              kColorIceSelectionElement,
          }},
         {PaletteId::kGlacier,
          {
              kColorGlacierPrimary,
              kColorGlacierShaderLayer1,
              kColorGlacierShaderLayer2,
              kColorGlacierShaderLayer3,
              kColorGlacierShaderLayer4,
              kColorGlacierShaderLayer5,
              kColorGlacierScrim,
              kColorGlacierSurfaceContainerHighestLight,
              kColorGlacierSurfaceContainerHighestDark,
              kColorGlacierSelectionElement,
          }},
         {PaletteId::kSapphire,
          {
              kColorSapphirePrimary,
              kColorSapphireShaderLayer1,
              kColorSapphireShaderLayer2,
              kColorSapphireShaderLayer3,
              kColorSapphireShaderLayer4,
              kColorSapphireShaderLayer5,
              kColorSapphireScrim,
              kColorSapphireSurfaceContainerHighestLight,
              kColorSapphireSurfaceContainerHighestDark,
              kColorSapphireSelectionElement,
          }},
         {PaletteId::kLavender,
          {
              kColorLavenderPrimary,
              kColorLavenderShaderLayer1,
              kColorLavenderShaderLayer2,
              kColorLavenderShaderLayer3,
              kColorLavenderShaderLayer4,
              kColorLavenderShaderLayer5,
              kColorLavenderScrim,
              kColorLavenderSurfaceContainerHighestLight,
              kColorLavenderSurfaceContainerHighestDark,
              kColorLavenderSelectionElement,
          }}});

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_COLORS_H_
