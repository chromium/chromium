// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_MOJOM_TRAITS_H_

#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom-shared.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

namespace mojo {

template <>
struct EnumTraits<side_panel::mojom::CustomizeChromeSection,
                  CustomizeChromeSection> {
  static side_panel::mojom::CustomizeChromeSection ToMojom(
      CustomizeChromeSection input) {
    return kToMojomMap.at(input);
  }

  static CustomizeChromeSection FromMojom(
      side_panel::mojom::CustomizeChromeSection input) {
    return kFromMojomMap.at(input);
  }

 private:
  static constexpr auto kToMojomMap =
      base::MakeFixedFlatMap<CustomizeChromeSection,
                             side_panel::mojom::CustomizeChromeSection>(
          {{CustomizeChromeSection::kUnspecified,
            side_panel::mojom::CustomizeChromeSection::kUnspecified},
           {CustomizeChromeSection::kAppearance,
            side_panel::mojom::CustomizeChromeSection::kAppearance},
           {CustomizeChromeSection::kShortcuts,
            side_panel::mojom::CustomizeChromeSection::kShortcuts},
           {CustomizeChromeSection::kModules,
            side_panel::mojom::CustomizeChromeSection::kModules},
           {CustomizeChromeSection::kWallpaperSearch,
            side_panel::mojom::CustomizeChromeSection::kWallpaperSearch},
           {CustomizeChromeSection::kToolbar,
            side_panel::mojom::CustomizeChromeSection::kToolbar},
           {CustomizeChromeSection::kFooter,
            side_panel::mojom::CustomizeChromeSection::kFooter}});

  static constexpr auto kFromMojomMap =
      base::MakeFixedFlatMap<side_panel::mojom::CustomizeChromeSection,
                             CustomizeChromeSection>(
          kToMojomMap,
          /*comp=*/{},
          [](const auto& entry) {
            return std::pair(entry.second, entry.first);
          });
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_MOJOM_TRAITS_H_
