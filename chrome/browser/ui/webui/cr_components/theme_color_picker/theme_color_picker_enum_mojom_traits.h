// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_THEME_COLOR_PICKER_ENUM_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_THEME_COLOR_PICKER_ENUM_MOJOM_TRAITS_H_

#include "chrome/browser/themes/theme_service.h"

namespace mojo {

template <>
struct EnumTraits<theme_color_picker::mojom::BrowserColorVariant,
                  ThemeService::BrowserColorVariant> {
  static theme_color_picker::mojom::BrowserColorVariant ToMojom(
      ThemeService::BrowserColorVariant input) {
    switch (input) {
      case ThemeService::BrowserColorVariant::kSystem:
        return theme_color_picker::mojom::BrowserColorVariant::kSystem;
      case ThemeService::BrowserColorVariant::kTonalSpot:
        return theme_color_picker::mojom::BrowserColorVariant::kTonalSpot;
      case ThemeService::BrowserColorVariant::kNeutral:
        return theme_color_picker::mojom::BrowserColorVariant::kNeutral;
      case ThemeService::BrowserColorVariant::kVibrant:
        return theme_color_picker::mojom::BrowserColorVariant::kVibrant;
      case ThemeService::BrowserColorVariant::kExpressive:
        return theme_color_picker::mojom::BrowserColorVariant::kExpressive;
    }

    NOTREACHED_NORETURN();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(theme_color_picker::mojom::BrowserColorVariant input,
                        ThemeService::BrowserColorVariant* output) {
    switch (input) {
      case theme_color_picker::mojom::BrowserColorVariant::kSystem:
        *output = ThemeService::BrowserColorVariant::kSystem;
        return true;
      case theme_color_picker::mojom::BrowserColorVariant::kTonalSpot:
        *output = ThemeService::BrowserColorVariant::kTonalSpot;
        return true;
      case theme_color_picker::mojom::BrowserColorVariant::kNeutral:
        *output = ThemeService::BrowserColorVariant::kNeutral;
        return true;
      case theme_color_picker::mojom::BrowserColorVariant::kVibrant:
        *output = ThemeService::BrowserColorVariant::kVibrant;
        return true;
      case theme_color_picker::mojom::BrowserColorVariant::kExpressive:
        *output = ThemeService::BrowserColorVariant::kExpressive;
        return true;
    }

    return false;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_THEME_COLOR_PICKER_ENUM_MOJOM_TRAITS_H_
