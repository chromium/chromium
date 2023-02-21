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
  mixer[kColorSidePanelBadgeBackground] = {ui::kColorSysSurfaceVariant};
  mixer[kColorSidePanelBadgeBackgroundUpdated] = {
      ui::kColorSysTertiaryContainer};
  mixer[kColorSidePanelBadgeForeground] = {ui::kColorSysOnSurfaceVariant};
  mixer[kColorSidePanelBadgeForegroundUpdated] = {
      ui::kColorSysOnTertiaryContainer};

  // TODO(crbug.com/1400859): Finalize filter chip colors.
  mixer[kColorSidePanelFilterChipBorder] = {ui::kColorButtonBackgroundTonal};
  mixer[kColorSidePanelFilterChipForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelFilterChipForegroundSelected] = {
      ui::kColorSysOnPrimaryContainer};
  mixer[kColorSidePanelFilterChipIcon] = {ui::kColorSysPrimary};
  mixer[kColorSidePanelFilterChipIconSelected] = {
      ui::kColorSysOnPrimaryContainer};
  mixer[kColorSidePanelFilterChipBackgroundSelected] = {
      ui::kColorSysPrimaryContainer};

  // TODO(crbug.com/1400860): Change to kColorSysNeutralOutline once available
  mixer[kColorSidePanelTextfieldBorder] = {
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark
          ? ui::kColorRefNeutral40
          : ui::kColorRefNeutral80};
}
