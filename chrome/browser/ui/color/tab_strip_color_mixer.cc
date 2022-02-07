// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/tab_strip_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

void AddTabStripColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  if (key.contrast_mode == ui::ColorProviderManager::ContrastMode::kNormal) {
    mixer[kColorTabForegroundActiveFrameActive] = {kColorToolbarText};
  }
  mixer[kColorTabForegroundActiveFrameInactive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTabBackgroundActiveFrameActive] = {kColorToolbar};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorFrameActive};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {ui::kColorFrameInactive};
}
