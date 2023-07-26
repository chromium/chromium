// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_ENUM_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_ENUM_MOJOM_TRAITS_H_

#include "chrome/browser/themes/theme_service.h"

namespace mojo {

template <>
struct EnumTraits<side_panel::mojom::BrowserColorVariant,
                  ThemeService::BrowserColorVariant> {
  static side_panel::mojom::BrowserColorVariant ToMojom(
      ThemeService::BrowserColorVariant input) {
    switch (input) {
      case ThemeService::BrowserColorVariant::kSystem:
        return side_panel::mojom::BrowserColorVariant::kSystem;
      case ThemeService::BrowserColorVariant::kTonalSpot:
        return side_panel::mojom::BrowserColorVariant::kTonalSpot;
      case ThemeService::BrowserColorVariant::kNeutral:
        return side_panel::mojom::BrowserColorVariant::kNeutral;
      case ThemeService::BrowserColorVariant::kVibrant:
        return side_panel::mojom::BrowserColorVariant::kVibrant;
      case ThemeService::BrowserColorVariant::kExpressive:
        return side_panel::mojom::BrowserColorVariant::kExpressive;
    }

    NOTREACHED_NORETURN();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(side_panel::mojom::BrowserColorVariant input,
                        ThemeService::BrowserColorVariant* output) {
    switch (input) {
      case side_panel::mojom::BrowserColorVariant::kSystem:
        *output = ThemeService::BrowserColorVariant::kSystem;
        return true;
      case side_panel::mojom::BrowserColorVariant::kTonalSpot:
        *output = ThemeService::BrowserColorVariant::kTonalSpot;
        return true;
      case side_panel::mojom::BrowserColorVariant::kNeutral:
        *output = ThemeService::BrowserColorVariant::kNeutral;
        return true;
      case side_panel::mojom::BrowserColorVariant::kVibrant:
        *output = ThemeService::BrowserColorVariant::kVibrant;
        return true;
      case side_panel::mojom::BrowserColorVariant::kExpressive:
        *output = ThemeService::BrowserColorVariant::kExpressive;
        return true;
    }

    NOTREACHED_NORETURN();
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_ENUM_MOJOM_TRAITS_H_
