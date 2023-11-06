// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_CUSTOMIZE_CHROME_COLORS_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_CUSTOMIZE_CHROME_COLORS_H_

#include <array>

#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "ui/base/mojom/themes.mojom.h"

// The customize chrome side panel only uses the chrome colors with the
// following ids, which is a subset of all chrome colors.
constexpr int kCustomizeChromeColorIds[] = {
    2,   // Cool grey.
    3,   // Midnight blue.
    4,   // Black.
    17,  // Dark pink and red.
    20,  // Dark teal.
    21,  // Dark blue.
    22,  // Dark purple.
    16,  // Pink.
    13,  // Light green.
    14,  // Light teal.
    15,  // Light blue.
    6,   // Yellow and white.
    5,   // Beige and white.
    1,   // Warm grey.
};

// The chrome colors selected and ordered by |kCustomizeChromeColorIds|.
// Generated at compile time.
extern const std::array<chrome_colors::ColorInfo,
                        std::size(kCustomizeChromeColorIds)>
    kCustomizeChromeColors;

struct DynamicColorInfo {
  constexpr DynamicColorInfo(int id,
                             SkColor color,
                             int label_id,
                             ui::mojom::BrowserColorVariant variant)
      : id(id), color(color), label_id(label_id), variant(variant) {}
  int id;
  SkColor color;
  int label_id;
  ui::mojom::BrowserColorVariant variant;
};

extern const std::array<DynamicColorInfo, 13> kDynamicCustomizeChromeColors;

SkColor HueToSkColor(float hue);

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_CUSTOMIZE_CHROME_COLORS_H_
