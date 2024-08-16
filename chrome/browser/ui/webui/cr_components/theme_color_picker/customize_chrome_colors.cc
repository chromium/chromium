// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/cr_components/theme_color_picker/customize_chrome_colors.h"

#include <array>
#include <utility>

#include "chrome/browser/new_tab_page/chrome_colors/generated_colors_info.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/color/color_provider_utils.h"

namespace {

// Returns chrome color with ID |kCustomizeChromeColorIds[I]|.
template <size_t I>
constexpr chrome_colors::ColorInfo GetChromeColor() {
  constexpr int id = kCustomizeChromeColorIds[I];
  // We assume that chrome colors are stored sequentially ordered by their ID
  // starting at ID 1.
  constexpr auto chrome_color = chrome_colors::kGeneratedColorsInfo[id - 1];
  static_assert(chrome_color.id == id);
  return chrome_color;
}

template <std::size_t... I>
constexpr auto MakeCustomizeChromeColors(std::index_sequence<I...>) {
  return std::array<chrome_colors::ColorInfo, sizeof...(I)>{
      GetChromeColor<I>()...};
}

}  // namespace

const decltype(kCustomizeChromeColors) kCustomizeChromeColors =
    MakeCustomizeChromeColors(
        std::make_index_sequence<std::size(kCustomizeChromeColorIds)>{});

const decltype(kDynamicCustomizeChromeColors) kDynamicCustomizeChromeColors =
    std::array<DynamicColorInfo, 13>{
        // ID 0 reserved for other colors.
        // ID 1 reserved for grayscale theme.
        DynamicColorInfo(/*id=*/2,
                         SkColorSetRGB(140, 171, 228),
                         IDS_NTP_COLORS_BLUE,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
        DynamicColorInfo(/*id=*/3,
                         SkColorSetRGB(140, 171, 228),
                         IDS_NTP_COLORS_COOL_GREY,
                         ui::mojom::BrowserColorVariant::kNeutral),
        DynamicColorInfo(/*id=*/4,
                         SkColorSetRGB(136, 136, 136),
                         IDS_NTP_COLORS_GREY,
                         ui::mojom::BrowserColorVariant::kNeutral),
        DynamicColorInfo(/*id=*/5,
                         SkColorSetRGB(38, 166, 154),
                         IDS_NTP_COLORS_AQUA,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
        DynamicColorInfo(/*id=*/6,
                         SkColorSetRGB(0, 255, 0),
                         IDS_NTP_COLORS_GREEN,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
        DynamicColorInfo(/*id=*/7,
                         SkColorSetRGB(135, 186, 129),
                         IDS_NTP_COLORS_VIRIDIAN,
                         ui::mojom::BrowserColorVariant::kNeutral),
        DynamicColorInfo(/*id=*/8,
                         SkColorSetRGB(250, 223, 115),
                         IDS_NTP_COLORS_CITRON,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
        DynamicColorInfo(/*id=*/9,
                         SkColorSetRGB(255, 128, 0),
                         IDS_NTP_COLORS_ORANGE,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
        DynamicColorInfo(/*id=*/10,
                         SkColorSetRGB(252, 219, 201),
                         IDS_NTP_COLORS_APRICOT,
                         ui::mojom::BrowserColorVariant::kNeutral),
        DynamicColorInfo(/*id=*/11,
                         SkColorSetRGB(243, 178, 190),
                         IDS_NTP_COLORS_ROSE,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
        DynamicColorInfo(/*id=*/12,
                         SkColorSetRGB(243, 178, 190),
                         IDS_NTP_COLORS_PINK,
                         ui::mojom::BrowserColorVariant::kNeutral),
        DynamicColorInfo(/*id=*/13,
                         SkColorSetRGB(255, 0, 255),
                         IDS_NTP_COLORS_FUCHSIA,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
        DynamicColorInfo(/*id=*/14,
                         SkColorSetRGB(229, 213, 252),
                         IDS_NTP_COLORS_VIOLET,
                         ui::mojom::BrowserColorVariant::kTonalSpot),
    };

SkColor HueToSkColor(float hue) {
  return color_utils::HSLToSkColor({std::clamp(hue / 360, 0.0f, 1.0f), 1, .5},
                                   255);
}
