// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixers.h"

// #include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/ui/color/chrome_color_mixer.h"
#include "chrome/browser/ui/color/omnibox_color_mixer.h"

void AddChromeColorMixers(ui::ColorProvider* provider,
                          ui::ColorProviderManager::ColorMode color_mode,
                          ui::ColorProviderManager::ContrastMode contrast_mode,
                          ui::ColorProviderManager::SystemTheme system_theme) {
  AddChromeColorMixer(provider);
  AddOmniboxColorMixer(
      provider, contrast_mode == ui::ColorProviderManager::ContrastMode::kHigh);
  // if (custom_theme)
  //   custom_theme->AddCustomThemeColorMixers(provider);
}
