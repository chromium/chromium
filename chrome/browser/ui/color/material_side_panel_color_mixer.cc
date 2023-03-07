// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_side_panel_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

void AddMaterialSidePanelColorMixer(ui::ColorProvider* provider,
                                    const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorSidePanelContentBackground] = {ui::kColorSysSurface4};
  mixer[kColorSidePanelScrollbarThumb] = {ui::kColorSysPrimary};
  mixer[kColorSidePanelCardBackground] = {ui::kColorSysSurface};

  mixer[kColorSidePanelBadgeBackground] = {ui::kColorSysSurfaceVariant};
  mixer[kColorSidePanelBadgeBackgroundUpdated] = {
      ui::kColorSysTertiaryContainer};
  mixer[kColorSidePanelBadgeForeground] = {ui::kColorSysOnSurfaceVariant};
  mixer[kColorSidePanelBadgeForegroundUpdated] = {
      ui::kColorSysOnTertiaryContainer};

  mixer[kColorSidePanelFilterChipBorder] = {ui::kColorSysTonalOutline};
  mixer[kColorSidePanelFilterChipForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelFilterChipForegroundSelected] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorSidePanelFilterChipIcon] = {ui::kColorSysPrimary};
  mixer[kColorSidePanelFilterChipIconSelected] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorSidePanelFilterChipBackgroundSelected] = {
      ui::kColorSysTonalContainer};

  mixer[kColorSidePanelTextfieldBorder] = {ui::kColorSysNeutralOutline};
}
