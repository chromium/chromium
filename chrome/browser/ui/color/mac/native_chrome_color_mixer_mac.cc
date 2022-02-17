// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderManager::Key& key) {
  if (key.contrast_mode != ui::ColorProviderManager::ContrastMode::kHigh)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  mixer[kColorLocationBarBorder] = {dark_mode ? SK_ColorWHITE : SK_ColorBLACK};
}
