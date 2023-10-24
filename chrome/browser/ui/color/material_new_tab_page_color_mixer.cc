// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_new_tab_page_color_mixer.h"

#include "base/logging.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "components/search/ntp_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

void AddMaterialNewTabPageColorMixer(ui::ColorProvider* provider,
                                     const ui::ColorProviderKey& key) {
  if (!ShouldApplyChromeMaterialOverrides(key) ||
      !features::IsChromeWebuiRefresh2023()) {
    return;
  }

  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorNewTabPageActiveBackground] = {
      ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorNewTabPageAddShortcutBackground] = {ui::kColorSysTonalContainer};
  mixer[kColorNewTabPageAddShortcutForeground] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorNewTabPageBackground] = {ui::kColorSysBase};
  mixer[kColorNewTabPageBorder] = {ui::kColorSysBaseContainer};
  mixer[kColorNewTabPageButtonBackground] = {ui::kColorSysTonalContainer};
  mixer[kColorNewTabPageButtonBackgroundHovered] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorNewTabPageButtonForeground] = {ui::kColorSysOnTonalContainer};

  mixer[kColorNewTabPageControlBackgroundHovered] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorNewTabPageFocusRing] = {ui::kColorSysStateFocusRing};
  mixer[kColorNewTabPageLink] = {ui::kColorSysPrimary};
  mixer[kColorNewTabPageLogo] = {ui::kColorSysPrimary};

  mixer[kColorNewTabPageMostVisitedTileBackground] = {
      ui::kColorSysSurfaceVariant};
  mixer[kColorNewTabPageMostVisitedForeground] = {ui::kColorSysOnSurfaceSubtle};

  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpComprehensiveThemeRealbox)) {
    mixer[kColorRealboxBackground] = {ui::kColorSysBase};
    mixer[kColorRealboxBackgroundHovered] = {ui::kColorSysStateHoverOnSubtle};
    mixer[kColorRealboxForeground] = {ui::kColorSysOnSurfaceSubtle};
  }

  mixer[kColorNewTabPageHistoryClustersModuleItemBackground] = {
      ui::kColorSysBaseContainerElevated};

  mixer[kColorNewTabPageModuleBackground] = {ui::kColorSysBaseContainer};
  if (base::FeatureList::IsEnabled(ntp_features::kNtpModulesRedesigned)) {
    mixer[kColorNewTabPageModuleItemBackground] = {
        ui::kColorSysBaseContainerElevated};
    mixer[kColorNewTabPageModuleItemBackgroundHovered] = {
        ui::kColorSysStateHoverBrightBlendProtection};
  } else {
    mixer[kColorNewTabPageModuleItemBackground] = {ui::kColorSysBaseContainer};
  }
  mixer[kColorNewTabPageModuleElementDivider] = {ui::kColorSysDivider};
  mixer[kColorNewTabPageModuleContextMenuDivider] = {ui::kColorSysDivider};

  mixer[kColorNewTabPagePromoBackground] = {ui::kColorSysBase};
  mixer[kColorNewTabPagePrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorNewTabPageSecondaryForeground] = {ui::kColorSysOnSurfaceSubtle};
}
