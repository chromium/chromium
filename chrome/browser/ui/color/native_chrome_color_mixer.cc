// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_utils.h"

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN)
void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderManager::Key& key) {
#if !BUILDFLAG(IS_ANDROID)
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorTabBackgroundInactiveFrameActive] =
      ui::HSLShift({ui::kColorFrameActive},
                   GetThemeTint(ThemeProperties::TINT_BACKGROUND_TAB, key));
  mixer[kColorTabBackgroundInactiveFrameInactive] =
      ui::HSLShift({ui::kColorFrameInactive},
                   GetThemeTint(ThemeProperties::TINT_BACKGROUND_TAB, key));
#endif
}
#endif
