// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom-shared.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

namespace mojo {

template <>
struct EnumTraits<side_panel::mojom::CustomizeChromeSection,
                  CustomizeChromeSection> {
  static side_panel::mojom::CustomizeChromeSection ToMojom(
      CustomizeChromeSection input) {
    static constexpr auto section_map =
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
    return section_map.at(input);
  }

  static bool FromMojom(side_panel::mojom::CustomizeChromeSection input,
                        CustomizeChromeSection* out) {
    static constexpr auto section_map =
        base::MakeFixedFlatMap<side_panel::mojom::CustomizeChromeSection,
                               CustomizeChromeSection>(
            {{side_panel::mojom::CustomizeChromeSection::kUnspecified,
              CustomizeChromeSection::kUnspecified},
             {side_panel::mojom::CustomizeChromeSection::kAppearance,
              CustomizeChromeSection::kAppearance},
             {side_panel::mojom::CustomizeChromeSection::kShortcuts,
              CustomizeChromeSection::kShortcuts},
             {side_panel::mojom::CustomizeChromeSection::kModules,
              CustomizeChromeSection::kModules},
             {side_panel::mojom::CustomizeChromeSection::kWallpaperSearch,
              CustomizeChromeSection::kWallpaperSearch},
             {side_panel::mojom::CustomizeChromeSection::kToolbar,
              CustomizeChromeSection::kToolbar},
             {side_panel::mojom::CustomizeChromeSection::kFooter,
              CustomizeChromeSection::kFooter}});
    *out = section_map.at(input);
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_MOJOM_TRAITS_H_
