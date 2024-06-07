// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_new_tab_page_color_mixer.h"

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "components/search/ntp_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

void AddMaterialNewTabPageColorMixer(ui::ColorProvider* provider,
                                     const ui::ColorProviderKey& key) {
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }
  const bool dark_mode =
      key.color_mode == ui::ColorProviderKey::ColorMode::kDark;

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

  mixer[kColorNewTabPageHistoryClustersModuleItemBackground] = {
      ui::kColorSysBaseContainerElevated};

  mixer[kColorNewTabPageModuleBackground] = {ui::kColorSysBaseContainer};
  mixer[kColorNewTabPageModuleIconBackground] = {ui::kColorSysNeutralContainer};
  // Styling for Doodle Share Button.
  mixer[kColorNewTabPageDoodleShareButtonBackground] = {
      ui::kColorSysNeutralContainer};
  mixer[kColorNewTabPageDoodleShareButtonIcon] = {ui::kColorSysOnSurface};

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

  mixer[kColorNewTabPageModuleCalendarEventCollapsedBackground] = {
      ui::kColorSysSurface1};
  mixer[kColorNewTabPageModuleCalendarEventExpandedBackground] = {
      ui::kColorSysSurface};
  mixer[kColorNewTabPageModuleCalendarEventTimeStatusBackground] = {
      ui::kColorSysNeutralContainer};

  mixer[kColorNewTabPagePromoBackground] = {ui::kColorSysBase};
  mixer[kColorNewTabPagePrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorNewTabPageSecondaryForeground] = {ui::kColorSysOnSurfaceSubtle};

  mixer[kColorNewTabPageWallpaperSearchButtonBackground] = {
      ui::kColorSysPrimary};
  mixer[kColorNewTabPageWallpaperSearchButtonBackgroundHovered] = {
      kColorNewTabPageButtonBackgroundHovered};
  mixer[kColorNewTabPageWallpaperSearchButtonForeground] = {
      ui::kColorSysOnPrimary};
  if (base::FeatureList::IsEnabled(ntp_features::kRealboxCr23Theming) ||
      base::FeatureList::IsEnabled(ntp_features::kRealboxCr23All)) {
    // Steady state theme colors.
    mixer[kColorRealboxBackground] = {kColorToolbarBackgroundSubtleEmphasis};
    mixer[kColorRealboxBackgroundHovered] = {
        kColorToolbarBackgroundSubtleEmphasisHovered};
    mixer[kColorRealboxPlaceholder] = {kColorOmniboxTextDimmed};
    mixer[kColorRealboxSearchIconBackground] = {kColorOmniboxResultsIcon};
    mixer[kColorRealboxLensVoiceIconBackground] = {ui::kColorSysPrimary};
    mixer[kColorRealboxSelectionBackground] = {
        kColorOmniboxSelectionBackground};
    mixer[kColorRealboxSelectionForeground] = {
        kColorOmniboxSelectionForeground};

    // Expanded state theme colors.
    mixer[kColorRealboxAnswerIconBackground] = {
        kColorOmniboxAnswerIconGM3Background};
    mixer[kColorRealboxAnswerIconForeground] = {
        kColorOmniboxAnswerIconGM3Foreground};
    mixer[kColorRealboxForeground] = {kColorOmniboxText};
    mixer[kColorRealboxResultsActionChip] = {ui::kColorSysTonalOutline};
    mixer[kColorRealboxResultsActionChipIcon] = {ui::kColorSysPrimary};
    mixer[kColorRealboxResultsActionChipFocusOutline] = {
        ui::kColorSysStateFocusRing};
    mixer[kColorRealboxResultsBackgroundHovered] = {
        kColorOmniboxResultsBackgroundHovered};
    mixer[kColorRealboxResultsButtonHover] = {
        kColorOmniboxResultsButtonInkDropRowHovered};
    mixer[kColorRealboxResultsDimSelected] = {
        kColorOmniboxResultsTextDimmedSelected};
    mixer[kColorRealboxResultsFocusIndicator] = {
        kColorOmniboxResultsFocusIndicator};
    mixer[kColorRealboxResultsForeground] = {kColorOmniboxText};
    mixer[kColorRealboxResultsForegroundDimmed] = {kColorOmniboxTextDimmed};
    mixer[kColorRealboxResultsIcon] = {kColorOmniboxResultsIcon};
    mixer[kColorRealboxResultsIconSelected] = {kColorOmniboxResultsIcon};
    mixer[kColorRealboxResultsIconFocusedOutline] = {
        kColorOmniboxResultsButtonIconSelected};
    mixer[kColorRealboxResultsUrl] = {kColorOmniboxResultsUrl};
    mixer[kColorRealboxResultsUrlSelected] = {kColorOmniboxResultsUrlSelected};
    mixer[kColorRealboxShadow] =
        ui::SetAlpha(gfx::kGoogleGrey900,
                     (dark_mode ? /* % opacity */ 0.32 : 0.1) * SK_AlphaOPAQUE);

    // This determines weather the realbox expanded state background in dark
    // mode will match the omnibox or not.
    if (dark_mode &&
        !ntp_features::kNtpRealboxCr23ExpandedStateBgMatchesOmnibox.Get()) {
      mixer[kColorRealboxResultsBackground] = {
          kColorToolbarBackgroundSubtleEmphasis};
    } else {
      mixer[kColorRealboxResultsBackground] = {kColorOmniboxResultsBackground};
    }
  }
}
