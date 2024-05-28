// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"

#include "chrome/browser/themes/theme_service.h"
#include "components/lens/lens_features.h"
#include "ui/native_theme/native_theme.h"

namespace lens {

bool LensOverlayShouldUseDarkMode(ThemeService* theme_service) {
  if (!lens::features::UseBrowserDarkModeSettingForLensOverlay()) {
    return false;
  }
  ThemeService::BrowserColorScheme color_scheme =
      theme_service->GetBrowserColorScheme();
  return color_scheme == ThemeService::BrowserColorScheme::kSystem
             ? ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
             : color_scheme == ThemeService::BrowserColorScheme::kDark;
}

}  // namespace lens
