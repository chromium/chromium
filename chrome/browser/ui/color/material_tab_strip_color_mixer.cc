// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_tab_strip_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

void AddMaterialTabStripColorMixer(ui::ColorProvider* provider,
                                   const ui::ColorProviderManager::Key& key) {
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }

  // TODO(crbug.com/1399942): Validate final mappings for Gm3 color.
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorTabBackgroundActiveFrameActive] = {ui::kColorSysBase};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorSysHeader};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {
      ui::kColorSysHeaderInactive};

  mixer[kColorTabForegroundActiveFrameActive] = {ui::kColorSysOnSurface};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      ui::kColorSysOnSurfaceSecondary};
  mixer[kColorTabForegroundInactiveFrameInactive] = {
      kColorTabForegroundInactiveFrameActive};
}
