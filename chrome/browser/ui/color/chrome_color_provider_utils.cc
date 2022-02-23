// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_provider_utils.h"

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"

#include "ui/color/color_id_map_macros.inc"

std::string ChromeColorIdName(ui::ColorId color_id) {
  static constexpr const auto color_id_map =
      base::MakeFixedFlatMap<ui::ColorId, const char*>({CHROME_COLOR_IDS});
  auto* i = color_id_map.find(color_id);
  if (i != color_id_map.cend())
    return {i->second};
  NOTREACHED();
  return "<invalid>";
}

color_utils::HSL GetThemeTint(int id,
                              const ui::ColorProviderManager::Key& key) {
#if !BUILDFLAG(IS_ANDROID)
  color_utils::HSL hsl;
  if (key.custom_theme && key.custom_theme->GetTint(id, &hsl))
    return hsl;
  return ThemeProperties::GetDefaultTint(
      id, false, key.color_mode == ui::ColorProviderManager::ColorMode::kDark);
#else
  return {-1, -1, -1};
#endif  // !BUILDFLAG(IS_ANDROID)
}

#include "ui/color/color_id_map_macros.inc"
