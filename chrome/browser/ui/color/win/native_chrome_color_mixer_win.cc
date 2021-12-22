// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderManager::Key& key) {
  if (key.contrast_mode != ui::ColorProviderManager::ContrastMode::kHigh)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();

  // High contrast uses system colors.
  mixer[kColorOmniboxBackground] = {ui::kColorNativeBtnFace};
  mixer[kColorOmniboxText] = {ui::kColorNativeBtnText};
  mixer[kColorToolbar] = {ui::kColorNativeWindow};
  mixer[kColorToolbarText] = {ui::kColorNativeBtnText};
  mixer[kColorTabForegroundActiveFrameActive] = {ui::kColorNativeHighlightText};
}
