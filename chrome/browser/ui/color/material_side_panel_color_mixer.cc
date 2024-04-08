// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_side_panel_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

void AddMaterialSidePanelColorMixer(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorSidePanelContentBackground] = {ui::kColorSysBaseContainer};
  mixer[kColorSidePanelComboboxEntryIcon] = {ui::kColorSysPrimary};
  mixer[kColorSidePanelComboboxEntryTitle] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelEntryDropdownIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorSidePanelContentAreaSeparator] = {ui::kColorSysBaseContainer};

  // After ChromeRefresh2023 roll out these five should be moved to replace
  // their colors in c/b/ui/color/chrome_color_mixer.cc. For now they need a
  // separate themed ChromeRefresh2023 color because the side panel header has a
  // different background color than it did before.
  mixer[kColorSidePanelEntryIcon] = {kColorToolbarText};
  mixer[kColorSidePanelEntryTitle] = {kColorToolbarText};
  mixer[kColorSidePanelHeaderButtonIcon] = {kColorToolbarText};
  mixer[kColorSidePanelHeaderButtonIconDisabled] = {kColorToolbarTextDisabled};
  mixer[kColorSidePanelResizeAreaHandle] = {kColorToolbarText};

  mixer[kColorSidePanelHoverResizeAreaHandle] = {ui::kColorSysOutline};
  mixer[kColorSidePanelCardBackground] = {ui::kColorSysBaseContainerElevated};
  mixer[kColorSidePanelCardPrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelCardSecondaryForeground] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorSidePanelDivider] = {ui::kColorSysDivider};
  mixer[kColorSidePanelScrollbarThumb] = {ui::kColorSysTonalOutline};

  /* Dialogs within the side panel. */
  mixer[kColorSidePanelDialogBackground] = {ui::kColorSysSurface};
  mixer[kColorSidePanelDialogDivider] = {ui::kColorSysNeutralOutline};
  mixer[kColorSidePanelDialogPrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelDialogSecondaryForeground] = {
      ui::kColorSysOnSurfaceSubtle};

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
  mixer[kColorSidePanelCustomizeChromeClassicChromeTileBorder] = {
      ui::kColorSysTonalContainer};
  mixer[kColorSidePanelCustomizeChromeCornerNtpBorder] = {
      ui::kColorSysTonalContainer};
  mixer[kColorSidePanelCustomizeChromeCustomOptionBackground] = {
      ui::kColorSysTertiaryContainer};
  mixer[kColorSidePanelCustomizeChromeCustomOptionForeground] = {
      ui::kColorSysOnTertiaryContainer};
  mixer[kColorSidePanelCustomizeChromeMiniNtpActiveTab] = {ui::kColorSysBase};
  mixer[kColorSidePanelCustomizeChromeMiniNtpArrowsAndRefreshButton] = {
      ui::kColorSysOnSurfaceSecondary};
  mixer[kColorSidePanelCustomizeChromeMiniNtpBackground] = {ui::kColorSysBase};
  mixer[kColorSidePanelCustomizeChromeMiniNtpBorder] = {
      ui::kColorSysSurfaceVariant};
  mixer[kColorSidePanelCustomizeChromeMiniNtpCaron] = {
      ui::kColorSysOnSurfacePrimary};
  mixer[kColorSidePanelCustomizeChromeMiniNtpCaronContainer] = {
      ui::kColorSysHeaderContainer};
  mixer[kColorSidePanelCustomizeChromeMiniNtpChromeLogo] = {
      ui::kColorSysOnSurface};
  mixer[kColorSidePanelCustomizeChromeMiniNtpOmnibox] = {
      ui::kColorSysOmniboxContainer};
  mixer[kColorSidePanelCustomizeChromeMiniNtpTabStripBackground] = {
      ui::kColorSysHeader};
  mixer[kColorSidePanelCustomizeChromeThemeBackground] = {
      ui::kColorSysBaseContainerElevated};
  mixer[kColorSidePanelCustomizeChromeThemeCheckmarkBackground] = {
      ui::kColorSysPrimary};
  mixer[kColorSidePanelCustomizeChromeThemeCheckmarkForeground] = {
      ui::kColorSysOnPrimary};
  mixer[kColorSidePanelCustomizeChromeThemeSnapshotBackground] = {
      ui::kColorSysTonalContainer};
  mixer[kColorSidePanelCustomizeChromeWebStoreBorder] = {
      ui::kColorSysNeutralOutline};
  /*Customize Chrome Wallpaper Search*/
  mixer[kColorSidePanelWallpaperSearchErrorButtonBackground] = {
      ui::kColorSysTonalContainer};
  mixer[kColorSidePanelWallpaperSearchErrorButtonText] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorSidePanelWallpaperSearchTileBackground] = {ui::kColorSysSurface2};
  mixer[kColorSidePanelWallpaperSearchInspirationDescriptors] = {
      ui::kColorSysPrimary};

  /* Commerce */
  mixer[kColorSidePanelCommerceGraphAxis] = {ui::kColorSysDivider};
  mixer[kColorSidePanelCommerceGraphBubbleBackground] = {
      ui::kColorSysTonalContainer};
  mixer[kColorSidePanelCommerceGraphLine] = {ui::kColorLinkForegroundDefault};

  // Note anything below here will only apply if themes aren't being used.
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }
  mixer[kColorSidePanelEntryIcon] = {ui::kColorSysPrimary};
  mixer[kColorSidePanelEntryTitle] = {ui::kColorSysOnSurface};
  mixer[kColorSidePanelHeaderButtonIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorSidePanelHeaderButtonIconDisabled] = {ui::kColorSysStateDisabled};
  mixer[kColorSidePanelResizeAreaHandle] = {ui::kColorSysOnSurfaceSubtle};
}
