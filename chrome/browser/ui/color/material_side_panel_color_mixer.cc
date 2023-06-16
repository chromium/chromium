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
  mixer[kColorSidePanelContentBackground] = {ui::kColorSysBaseContainer};
  mixer[kColorSidePanelScrollbarThumb] = {ui::kColorSysPrimary};
  mixer[kColorSidePanelCardBackground] = {ui::kColorSysBaseContainerElevated};
  mixer[kColorSidePanelCardPrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelCardSecondaryForeground] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorSidePanelDivider] = {ui::kColorSysDivider};

  /* Dialogs within the side panel. */
  mixer[kColorSidePanelDialogBackground] = {ui::kColorSysSurface};
  mixer[kColorSidePanelDialogDivider] = {ui::kColorSysNeutralOutline};
  mixer[kColorSidePanelDialogPrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelDialogSecondaryForeground] = {
      ui::kColorSysOnSurfaceSubtle};

  /* Menus within the side panel. */
  mixer[kColorSidePanelMenuBackground] = {ui::kColorSysSurface};
  mixer[kColorSidePanelMenuDisabled] = {ui::kColorSysStateDisabled};
  mixer[kColorSidePanelMenuDivider] = {ui::kColorSysDivider};
  mixer[kColorSidePanelMenuForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelMenuIcon] = {ui::kColorSysOnSurfaceSubtle};

  mixer[kColorSidePanelBadgeBackground] = {ui::kColorSysNeutralContainer};
  mixer[kColorSidePanelBadgeBackgroundUpdated] = {
      ui::kColorSysTertiaryContainer};
  mixer[kColorSidePanelBadgeForeground] = {ui::kColorSysOnSurfaceVariant};
  mixer[kColorSidePanelBadgeForegroundUpdated] = {
      ui::kColorSysOnTertiaryContainer};

  mixer[kColorSidePanelEditFooterBorder] = {ui::kColorSysTonalOutline};

  mixer[kColorSidePanelFilterChipBorder] = {ui::kColorSysTonalOutline};
  mixer[kColorSidePanelFilterChipForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelFilterChipForegroundSelected] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorSidePanelFilterChipIcon] = {ui::kColorSysPrimary};
  mixer[kColorSidePanelFilterChipIconSelected] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorSidePanelFilterChipBackgroundHover] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorSidePanelFilterChipBackgroundSelected] = {
      ui::kColorSysTonalContainer};

  mixer[kColorSidePanelTextfieldBorder] = {ui::kColorSysNeutralOutline};

  /* Bookmarks */
  mixer[kColorSidePanelBookmarksSelectedFolderBackground] = {
      ui::kColorSysStateRipplePrimary};
  mixer[kColorSidePanelBookmarksSelectedFolderForeground] = {
      ui::kColorSysOnSurface};
  mixer[kColorSidePanelBookmarksSelectedFolderIcon] = {
      ui::kColorSysOnSurfaceSubtle};

  /* Customize Chrome */
  mixer[kColorSidePanelCustomizeChromeColorPickerCheckmarkBackground] = {
      ui::kColorSysOnSurface};
  mixer[kColorSidePanelCustomizeChromeColorPickerCheckmarkForeground] = {
      ui::kColorSysInverseOnSurface};
  mixer[kColorSidePanelCustomizeChromeColorPickerOptionBackground] = {
      ui::kColorSysNeutralContainer};
  mixer[kColorSidePanelCustomizeChromeCustomOptionBackground] = {
      ui::kColorSysTertiaryContainer};
  mixer[kColorSidePanelCustomizeChromeCustomOptionForeground] = {
      ui::kColorSysOnTertiaryContainer};
  mixer[kColorSidePanelCustomizeChromeThemeBackground] = {
      ui::kColorSysBaseContainerElevated};
  mixer[kColorSidePanelCustomizeChromeThemeCheckmarkBackground] = {
      ui::kColorSysPrimary};
  mixer[kColorSidePanelCustomizeChromeThemeCheckmarkForeground] = {
      ui::kColorSysOnPrimary};
  mixer[kColorSidePanelCustomizeChromeThemeSnapshotBackground] = {
      ui::kColorSysTonalContainer};
  mixer[kColorSidePanelCustomizeChromeWebStoreOptionBorder] = {
      ui::kColorSysNeutralOutline};
}
