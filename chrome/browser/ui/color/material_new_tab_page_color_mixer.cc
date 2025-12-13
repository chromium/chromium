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

namespace {

constexpr SkColor kColorSysSurface3_Light = SkColorSetRGB(0xEF, 0xF3, 0xFA);
constexpr SkColor kColorSysSurface_Light = SkColorSetRGB(0xFF, 0xFF, 0xFF);
constexpr SkColor kColorSysStateHoverOnSubtle_Light = SkColorSetARGB(0x0F, 0x1F, 0x1F, 0x1F);
constexpr SkColor kColorGemSysColorPrimary_Light =
    SkColorSetRGB(0x0B, 0x57, 0xD0);
constexpr SkColor kColorSysTonalOutline_Light = SkColorSetRGB(0xA8, 0xC7, 0xFA);
constexpr SkColor kColorSysPrimary_Light = SkColorSetRGB(0x0B, 0x57, 0xD0);

constexpr SkColor kColorSysOnSurfaceSubtle_Light =
    SkColorSetRGB(0x5E, 0x5E, 0x5E);
}  // namespace

void AddMaterialNewTabPageColorMixer(ui::ColorProvider* provider,
                                     const ui::ColorProviderKey& key) {
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }
  const bool dark_mode =
      key.color_mode == ui::ColorProviderKey::ColorMode::kDark;

  ui::ColorMixer& mixer = provider->AddMixer();
  // When adding a new color ID to this mixer, ensure it is ALSO added
  // to the GM2 color mixer.
  // LINT.IfChange
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

  mixer[kColorComposeboxBackground] = {SK_ColorWHITE};
  mixer[kColorComposeboxFileChipSpinner] = {kColorSysPrimary_Light};
  mixer[kColorComposeboxFont] = {
      dark_mode ? SkColorSetRGB(0xE6, 0xE8, 0xF0)
                : SkColorSetRGB(0x0A, 0x0A, 0x0A)};
  mixer[kColorComposeboxFontLight] = {
      SkColorSetRGB(0x1F, 0x1F, 0x1F)};
  mixer[kColorComposeboxCancelButton] = {
      dark_mode ? SkColorSetRGB(0xAD, 0xAF, 0xB8)
                : SkColorSetRGB(0x0A, 0x0A, 0x0A)};
  mixer[kColorComposeboxCancelButtonLight] = {ui::kColorRefNeutral30};
  mixer[kColorComposeboxErrorScrimBackground] = {
      dark_mode ? ui::SetAlpha({ui::kColorRefNeutral0}, 0xE6)
                : ui::SetAlpha({SkColorSetRGB(0xFF, 0xFF, 0xFF)}, 0xE6)};
  mixer[kColorComposeboxErrorScrimButtonBackground] = {
      ui::kColorSysPrimary};
  mixer[kColorComposeboxErrorScrimButtonBackgroundHover] = {
      ui::kColorSysStateHoverOnProminent};
  mixer[kColorComposeboxErrorScrimButtonText] = {
      ui::kColorSysOnPrimary};
  mixer[kColorComposeboxErrorScrimForeground] = {
      ui::kColorSysInverseSurface};
  mixer[kColorComposeboxHover] = {
      dark_mode ? SkColorSetRGB(0x25, 0x26, 0x2E)
                : SkColorSetRGB(0xE9, 0xEB, 0xF0)};
  mixer[kColorComposeboxInputIcon] = {ui::kColorRefNeutral30};
  mixer[kColorComposeboxLensButton] = {
      dark_mode ? SkColorSetRGB(0xAD, 0xAF, 0xB8)
                : SkColorSetRGB(0x0A, 0x0A, 0x0A)};
  mixer[kColorComposeboxOutlineHcm] = {
      dark_mode ? SkColorSetRGB(0xFF, 0xFF, 0xFF)
                : SkColorSetRGB(0x00, 0x00, 0x00)};
  mixer[kColorComposeboxRecentTabChipOutline] = {kColorSysTonalOutline_Light};
  mixer[kColorComposeboxScrimBackground] = {ui::kColorSysBase};
  mixer[kColorComposeboxSubmitButtonBackground] = {
      SkColorSetRGB(0x0B, 0x50, 0xD0)};
  mixer[kColorComposeboxSuggestionActivity] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorComposeboxTabSelectorButtonSelected] = {
      kColorGemSysColorPrimary_Light};
  mixer[kColorComposeboxTypeAhead] = {
      ui::SetAlpha({ui::kColorRefNeutral10}, 0x60)};
  mixer[kColorComposeboxTypeAheadChip] = {
      ui::SetAlpha({ui::kColorRefNeutral10}, 0x1E)};
  mixer[kColorComposeboxUploadButton] = {ui::kColorRefNeutral10};
  mixer[kColorComposeboxUploadButtonDisabled] = {
      dark_mode ? SkColorSetRGB(0x56, 0x59, 0x5E)
                : SkColorSetRGB(0xAD, 0xAF, 0xB8)};
  mixer[kColorComposeboxFileChipBackground] = {kColorSysSurface3_Light};
  mixer[kColorComposeboxFileChipFaviconBackground] = {kColorSysSurface_Light};
  mixer[kColorComposeboxFileChipText] = {
      SkColorSetRGB(0x1F, 0x1F, 0x1F)};
  mixer[kColorComposeboxPdfChipIcon] = {
      dark_mode ? SkColorSetRGB(0xAD, 0xAF, 0xB8)
                : SkColorSetRGB(0x56, 0x59, 0x5E)};
  mixer[kColorComposeboxFileImageOverlay] = {
      SkColorSetARGB(0x99, 0x00, 0x00, 0x00)};
  mixer[kColorComposeboxFileCarouselDivider] = {
      SkColorSetRGB(0xD3, 0xE3, 0xFD)};
  mixer[kColorComposeboxFileCarouselRemoveButton] = {kColorSysSurface_Light};
  mixer[kColorComposeboxFileCarouselUrl] = {kColorSysOnSurfaceSubtle_Light};
  mixer[kColorComposeboxFileCarouselRemoveGradientStart] = {
      SkColorSetRGB(0xF0, 0xF4, 0xF9)};
  mixer[kColorComposeboxFileCarouselRemoveGradientEnd] = {
      SkColorSetARGB(0x00, 0xF0, 0xF4, 0xF9)};
  mixer[kColorComposeboxContextEntrypointTextDisabled] = {
      SkColorSetARGB(0x60, 0x1F, 0x1F, 0x1F)};
  mixer[kColorComposeboxContextEntrypointHoverBackground] = {
      SkColorSetARGB(0x0F, 0x1F, 0x1F, 0x1F)};
  mixer[kColorComposeboxLink] = {gfx::kGoogleBlue700};

  mixer[kColorNewTabPageControlBackgroundHovered] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorNewTabPageFocusRing] = {ui::kColorSysStateFocusRing};
  mixer[kColorNewTabPageLink] = {ui::kColorSysPrimary};
  mixer[kColorNewTabPageLogo] = {ui::kColorSysPrimary};

  if (base::FeatureList::IsEnabled(ntp_features::kNtpNextFeatures)) {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {
        ui::kColorSysBaseContainer};
  } else {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {
        ui::kColorSysSurfaceVariant};
  }
  mixer[kColorNewTabPageMostVisitedForeground] = {ui::kColorSysOnSurfaceSubtle};

  mixer[kColorNewTabPageHistoryClustersModuleItemBackground] = {
      ui::kColorSysBaseContainerElevated};

  // Action chips colors.
  mixer[kColorNewTabPageActionChipBackground] = {ui::kColorSysBaseContainer};
  mixer[kColorNewTabPageActionChipBackgroundHover] = {
      ui::kColorSysStateRippleNeutralOnSubtle};

  mixer[kColorNewTabPageActionChipTextTitle] = {ui::kColorSysOnSurface};
  mixer[kColorNewTabPageActionChipTextBody] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorNewTabPageActionChipDeepSearchIcon] = {
      ui::kColorSysOnSurfaceSubtle};

  mixer[kColorNewTabPageModuleBackground] = {ui::kColorSysBaseContainer};
  mixer[kColorNewTabPageModuleIconBackground] = {ui::kColorSysNeutralContainer};
  // Styling for Doodle Share Button.
  mixer[kColorNewTabPageDoodleShareButtonBackground] = {
      ui::kColorSysNeutralContainer};
  mixer[kColorNewTabPageDoodleShareButtonIcon] = {ui::kColorSysOnSurface};

  mixer[kColorNewTabPageModuleItemBackground] = {
      ui::kColorSysBaseContainerElevated};
  mixer[kColorNewTabPageModuleItemBackgroundHovered] = {
      ui::kColorSysStateHoverBrightBlendProtection};

  mixer[kColorNewTabPageModuleElementDivider] = {ui::kColorSysDivider};
  mixer[kColorNewTabPageModuleContextMenuDivider] = {ui::kColorSysDivider};

  mixer[kColorNewTabPageModuleCalendarEventTimeStatusBackground] = {
      ui::kColorSysNeutralContainer};
  mixer[kColorNewTabPageModuleCalendarAttachmentScrollbarThumb] = {
      ui::kColorSysTonalOutline};
  mixer[kColorNewTabPageModuleCalendarDividerColor] = {ui::kColorSysDivider};

  // Tab group colors.
  mixer[kColorNewTabPageModuleTabGroupsGrey] = {kColorTabGroupBookmarkBarGrey};
  mixer[kColorNewTabPageModuleTabGroupsBlue] = {kColorTabGroupBookmarkBarBlue};
  mixer[kColorNewTabPageModuleTabGroupsRed] = {kColorTabGroupBookmarkBarRed};
  mixer[kColorNewTabPageModuleTabGroupsYellow] = {
      kColorTabGroupBookmarkBarYellow};
  mixer[kColorNewTabPageModuleTabGroupsGreen] = {
      kColorTabGroupBookmarkBarGreen};
  mixer[kColorNewTabPageModuleTabGroupsPink] = {kColorTabGroupBookmarkBarPink};
  mixer[kColorNewTabPageModuleTabGroupsPurple] = {
      kColorTabGroupBookmarkBarPurple};
  mixer[kColorNewTabPageModuleTabGroupsCyan] = {kColorTabGroupBookmarkBarCyan};
  mixer[kColorNewTabPageModuleTabGroupsOrange] = {
      kColorTabGroupBookmarkBarOrange};

  mixer[kColorNewTabPageModuleTabGroupsDotGrey] = {
      kColorTabGroupTabStripFrameActiveGrey};
  mixer[kColorNewTabPageModuleTabGroupsDotBlue] = {
      kColorTabGroupTabStripFrameActiveBlue};
  mixer[kColorNewTabPageModuleTabGroupsDotRed] = {
      kColorTabGroupTabStripFrameActiveRed};
  mixer[kColorNewTabPageModuleTabGroupsDotYellow] = {
      kColorTabGroupTabStripFrameActiveYellow};
  mixer[kColorNewTabPageModuleTabGroupsDotGreen] = {
      kColorTabGroupTabStripFrameActiveGreen};
  mixer[kColorNewTabPageModuleTabGroupsDotPink] = {
      kColorTabGroupTabStripFrameActivePink};
  mixer[kColorNewTabPageModuleTabGroupsDotPurple] = {
      kColorTabGroupTabStripFrameActivePurple};
  mixer[kColorNewTabPageModuleTabGroupsDotOrange] = {
      kColorTabGroupTabStripFrameActiveOrange};

  mixer[kColorNewTabPagePromoBackground] = {ui::kColorSysBase};
  mixer[kColorNewTabPagePrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorNewTabPageCommonInputPlaceholder] = {SkColorSetARGB(0x60, 0x1F, 0x1F, 0x1F)};
  mixer[kColorNewTabPageRealboxNextIconHover] = {kColorSysStateHoverOnSubtle_Light};
  mixer[kColorNewTabPageSecondaryForeground] = {ui::kColorSysOnSurfaceSubtle};

  mixer[kColorNewTabPageWallpaperSearchButtonBackground] = {
      ui::kColorSysPrimary};
  mixer[kColorNewTabPageWallpaperSearchButtonBackgroundHovered] = {
      kColorNewTabPageButtonBackgroundHovered};
  mixer[kColorNewTabPageWallpaperSearchButtonForeground] = {
      ui::kColorSysOnPrimary};
  if (base::FeatureList::IsEnabled(ntp_features::kRealboxCr23Theming)) {
    // Steady state theme colors.
    mixer[kColorSearchboxBackground] = {kColorToolbarBackgroundSubtleEmphasis};
    mixer[kColorSearchboxBackgroundHovered] = {
        kColorToolbarBackgroundSubtleEmphasisHovered};
    mixer[kColorSearchboxPlaceholder] = {kColorOmniboxTextDimmed};
    mixer[kColorSearchboxSearchIconBackground] = {kColorOmniboxResultsIcon};
    mixer[kColorSearchboxLensVoiceIconBackground] = {ui::kColorSysPrimary};
    mixer[kColorSearchboxSelectionBackground] = {
        kColorOmniboxSelectionBackground};
    mixer[kColorSearchboxSelectionForeground] = {
        kColorOmniboxSelectionForeground};

    // Expanded state theme colors.
    mixer[kColorSearchboxAnswerIconBackground] = {
        kColorOmniboxAnswerIconGM3Background};
    mixer[kColorSearchboxAnswerIconForeground] = {
        kColorOmniboxAnswerIconGM3Foreground};
    mixer[kColorSearchboxForeground] = {kColorOmniboxText};
    mixer[kColorSearchboxResultsActionChip] = {ui::kColorSysTonalOutline};
    mixer[kColorSearchboxResultsActionChipIcon] = {ui::kColorSysPrimary};
    mixer[kColorSearchboxResultsActionChipFocusOutline] = {
        ui::kColorSysStateFocusRing};
    mixer[kColorSearchboxResultsBackgroundHovered] = {
        kColorOmniboxResultsBackgroundHovered};
    mixer[kColorSearchboxResultsButtonHover] = {
        kColorOmniboxResultsButtonInkDropRowHovered};
    mixer[kColorSearchboxResultsDimSelected] = {
        kColorOmniboxResultsTextDimmedSelected};
    mixer[kColorSearchboxResultsFocusIndicator] = {
        kColorOmniboxResultsFocusIndicator};
    mixer[kColorSearchboxResultsForeground] = {kColorOmniboxText};
    mixer[kColorSearchboxResultsForegroundDimmed] = {kColorOmniboxTextDimmed};
    mixer[kColorSearchboxResultsIcon] = {kColorOmniboxResultsIcon};
    mixer[kColorSearchboxResultsIconSelected] = {kColorOmniboxResultsIcon};
    mixer[kColorSearchboxResultsIconFocusedOutline] = {
        kColorOmniboxResultsButtonIconSelected};
    mixer[kColorSearchboxResultsUrl] = {kColorOmniboxResultsUrl};
    mixer[kColorSearchboxResultsUrlSelected] = {
        kColorOmniboxResultsUrlSelected};
    mixer[kColorSearchboxShadow] =
        ui::SetAlpha(gfx::kGoogleGrey900,
                     (dark_mode ? /* % opacity */ 0.32 : 0.1) * SK_AlphaOPAQUE);

    // This determines weather the realbox expanded state background in dark
    // mode will match the omnibox or not.
    if (dark_mode &&
        !ntp_features::kNtpRealboxCr23ExpandedStateBgMatchesOmnibox.Get()) {
      mixer[kColorSearchboxResultsBackground] = {
          kColorToolbarBackgroundSubtleEmphasis};
    } else {
      mixer[kColorSearchboxResultsBackground] = {
          kColorOmniboxResultsBackground};
    }
  }

  /* NewTabFooter */
  mixer[kColorNewTabFooterBackground] = {ui::kColorSysSurface2};
  mixer[kColorNewTabFooterText] = {ui::kColorSysOnSurface};
  mixer[kColorNewTabFooterLogoBackground] = {ui::kColorSysSurface};
  // LINT.ThenChange(//chrome/browser/ui/color/new_tab_page_color_mixer.cc)
}
