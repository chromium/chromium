// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/new_tab_page_color_mixer.h"

#include "base/logging.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "components/search/ntp_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

ui::ColorTransform IncreaseLightness(ui::ColorTransform input_transform,
                                     double percent) {
  const auto generator = [](ui::ColorTransform input_transform, double percent,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor color = input_transform.Run(input_color, mixer);
    color_utils::HSL result;
    color_utils::SkColorToHSL(color, &result);
    result.l += (1 - result.l) * percent;
    const SkColor result_color =
        color_utils::HSLToSkColor(result, SkColorGetA(color));
    DVLOG(2) << "ColorTransform IncreaseLightness:"
             << " Percent: " << percent
             << " Transform Color: " << ui::SkColorName(color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(input_transform), percent);
}

}  // namespace

void AddNewTabPageColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorNewTabPageBackground] = {kColorToolbar};
  mixer[kColorNewTabPageHeader] = {SkColorSetRGB(0x96, 0x96, 0x96)};
  mixer[kColorNewTabPageLogo] = {kColorNewTabPageLogoUnthemedLight};
  mixer[kColorNewTabPageLogoUnthemedDark] = {gfx::kGoogleGrey700};
  mixer[kColorNewTabPageLogoUnthemedLight] = {SkColorSetRGB(0xEE, 0xEE, 0xEE)};

  if (dark_mode) {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {gfx::kGoogleGrey900};
  } else {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {
        kColorNewTabPageMostVisitedTileBackgroundUnthemed};
  }
  mixer[kColorNewTabPageMostVisitedTileBackgroundUnthemed] = {
      gfx::kGoogleGrey100};
  mixer[kColorNewTabPageSectionBorder] =
      ui::SetAlpha(kColorNewTabPageHeader, 0x50);
  mixer[kColorNewTabPageSearchBoxBackground] = {
      kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorNewTabPageSearchBoxBackgroundHovered] = {
      kColorToolbarBackgroundSubtleEmphasisHovered};
  mixer[kColorNewTabPageText] = {dark_mode ? gfx::kGoogleGrey200
                                           : SK_ColorBLACK};
  mixer[kColorNewTabPageTextUnthemed] = {gfx::kGoogleGrey050};
  mixer[kColorNewTabPageTextLight] =
      IncreaseLightness(kColorNewTabPageText, 0.40);

  mixer[kColorRealboxBackground] = {SK_ColorWHITE};
  mixer[kColorRealboxBackgroundHovered] = {SK_ColorWHITE};
  mixer[kColorRealboxBorder] = {
      key.contrast_mode == ui::ColorProviderManager::ContrastMode::kHigh
          ? kColorLocationBarBorder
          : gfx::kGoogleGrey300};
  mixer[kColorRealboxForeground] = {SK_ColorBLACK};
  mixer[kColorRealboxPlaceholder] = {gfx::kGoogleGrey700};
  mixer[kColorRealboxResultsBackground] = {SK_ColorWHITE};
  mixer[kColorRealboxResultsBackgroundHovered] =
      ui::SetAlpha({gfx::kGoogleGrey900},
                   /* 10% opacity */ 0.1 * 255);
  mixer[kColorRealboxResultsControlBackgroundHovered] =
      ui::SetAlpha(gfx::kGoogleGrey900, /* 10% opacity */ 0.1 * 255);
  mixer[kColorRealboxResultsDimSelected] = {gfx::kGoogleGrey700};
  mixer[kColorRealboxResultsForeground] = {SK_ColorBLACK};
  mixer[kColorRealboxResultsForegroundDimmed] = {gfx::kGoogleGrey700};
  mixer[kColorRealboxResultsIconSelected] = {gfx::kGoogleGrey700};
  mixer[kColorRealboxResultsUrl] = {gfx::kGoogleBlue700};
  mixer[kColorRealboxResultsUrlSelected] = {gfx::kGoogleBlue700};
  mixer[kColorRealboxSearchIconBackground] = {gfx::kGoogleGrey700};
  mixer[kColorRealboxResultsIcon] = {gfx::kGoogleGrey700};
  mixer[kColorRealboxResultsIconFocusedOutline] = {gfx::kGoogleBlue600};

  if (base::FeatureList::IsEnabled(ntp_features::kRealboxMatchOmniboxTheme)) {
    mixer[kColorRealboxForeground] = {ui::kColorTextfieldForeground};
    mixer[kColorRealboxPlaceholder] = {kColorOmniboxTextDimmed};
    mixer[kColorRealboxResultsBackground] = {kColorOmniboxResultsBackground};
    mixer[kColorRealboxResultsBackgroundHovered] = {
        kColorOmniboxResultsBackgroundHovered};
    if (dark_mode) {
      mixer[kColorRealboxResultsControlBackgroundHovered] =
          ui::SetAlpha({gfx::kGoogleGrey200},
                       /* 10% opacity */ 0.1 * 255);
    }
    mixer[kColorRealboxResultsDimSelected] = {
        kColorOmniboxResultsBackgroundSelected};
    mixer[kColorRealboxResultsForeground] = {ui::kColorTextfieldForeground};
    mixer[kColorRealboxResultsForegroundDimmed] = {
        kColorOmniboxResultsTextDimmed};
    mixer[kColorRealboxResultsIconSelected] = {
        kColorOmniboxResultsIconSelected};
    mixer[kColorRealboxSearchIconBackground] = {kColorOmniboxResultsIcon};
    mixer[kColorRealboxResultsIcon] = {kColorOmniboxResultsIcon};
    mixer[kColorRealboxResultsUrl] = {kColorOmniboxResultsUrl};
    mixer[kColorRealboxResultsUrlSelected] = {kColorOmniboxResultsUrlSelected};

    // For details see `kRealboxMatchOmniboxThemeVariations` in
    // chrome/browser/about_flags.cc.
    switch (base::GetFieldTrialParamByFeatureAsInt(
        ntp_features::kRealboxMatchOmniboxTheme,
        ntp_features::kRealboxMatchOmniboxThemeVariantParam, 0)) {
      case 0:
        mixer[kColorRealboxBackground] = {
            kColorToolbarBackgroundSubtleEmphasis};
        mixer[kColorRealboxBackgroundHovered] = {
            kColorToolbarBackgroundSubtleEmphasisHovered};
        break;
      // NTP background on steady state and Omnibox steady state background on
      // hover.
      case 1:
        mixer[kColorRealboxBackground] = {kColorNewTabPageBackground};
        mixer[kColorRealboxBackgroundHovered] = {
            kColorToolbarBackgroundSubtleEmphasisHovered};
        break;
      // NTP background on steady state and Omnibox active state background on
      // hover.
      case 2:
        mixer[kColorRealboxBackground] = {kColorNewTabPageBackground};
        mixer[kColorRealboxBackgroundHovered] = {
            kColorOmniboxResultsBackground};
        break;
    }
  }

  AddWebThemeNewTabPageColors(mixer, dark_mode);
}

void AddWebThemeNewTabPageColors(ui::ColorMixer& mixer, bool dark_mode) {
  mixer[kColorNewTabPageActionButtonBorder] = {dark_mode ? gfx::kGoogleGrey700
                                                         : gfx::kGoogleGrey300};
  mixer[kColorNewTabPageActionButtonBorderHovered] = {
      dark_mode ? gfx::kGoogleGrey700 : gfx::kGoogleBlue100};
  mixer[kColorNewTabPageActiveBackground] =
      ui::SetAlpha({dark_mode ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900},
                   /* 16% opacity */ 0.16 * 255);
  mixer[kColorNewTabPageBackgroundOverride] = {dark_mode ? gfx::kGoogleGrey900
                                                         : SK_ColorWHITE};
  mixer[kColorNewTabPageBorder] = {dark_mode ? gfx::kGoogleGrey700
                                             : gfx::kGoogleGrey300};
  mixer[kColorNewTabPageChipBackground] = {dark_mode ? gfx::kGoogleBlue300
                                                     : gfx::kGoogleBlue600};
  mixer[kColorNewTabPageChipForeground] = {dark_mode ? gfx::kGoogleGrey900
                                                     : SK_ColorWHITE};
  mixer[kColorNewTabPageControlBackgroundHovered] =
      ui::SetAlpha({dark_mode ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900},
                   /* 10% opacity */ 0.1 * 255);
  mixer[kColorNewTabPageControlBackgroundSelected] =
      ui::SetAlpha({dark_mode ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600},
                   /* 24% opacity */ 0.24 * 255);
  mixer[kColorNewTabPageFocusShadow] =
      dark_mode
          ? ui::SetAlpha({gfx::kGoogleBlue300}, /* 50% opacity */ 0.5 * 255)
          : ui::SetAlpha({gfx::kGoogleBlue600}, /* 40% opacity */ 0.4 * 255);
  mixer[kColorNewTabPageIconButtonBackground] = {
      dark_mode ? SK_ColorWHITE : gfx::kGoogleGrey600};
  mixer[kColorNewTabPageIconButtonBackgroundActive] = {
      dark_mode ? gfx::kGoogleGrey300 : gfx::kGoogleGrey700};
  mixer[kColorNewTabPageLink] = {dark_mode ? gfx::kGoogleBlue300
                                           : SkColorSetRGB(0x06, 0x37, 0x74)};
  mixer[kColorNewTabPageMicBorderColor] = {dark_mode ? gfx::kGoogleGrey100
                                                     : gfx::kGoogleGrey300};
  mixer[kColorNewTabPageMicIconColor] = {dark_mode ? gfx::kGoogleGrey100
                                                   : gfx::kGoogleGrey700};
  mixer[kColorNewTabPageModuleIconContainerBackground] =
      ui::SetAlpha({dark_mode ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600},
                   /* 24% opacity */ 0.24 * 255);
  mixer[kColorNewTabPageModuleScrollButtonBackground] = {
      dark_mode ? gfx::kGoogleGrey700 : gfx::kGoogleGrey100};
  mixer[kColorNewTabPageModuleScrollButtonBackgroundHovered] = {
      dark_mode ? gfx::kGoogleGrey600 : gfx::kGoogleGrey300};
  mixer[kColorNewTabPagePrimaryForeground] = {dark_mode ? gfx::kGoogleGrey200
                                                        : gfx::kGoogleGrey900};
  mixer[kColorNewTabPageSecondaryForeground] = {
      dark_mode ? gfx::kGoogleGrey500 : gfx::kGoogleGrey700};
  mixer[kColorNewTabPageSelectedBackground] =
      ui::SetAlpha({dark_mode ? gfx::kGoogleBlue300 : gfx::kGoogleBlue700},
                   /* 16% opacity */ 0.16 * 255);
  mixer[kColorNewTabPageSelectedBorder] = {dark_mode ? gfx::kGoogleBlue300
                                                     : gfx::kGoogleBlue600};
  mixer[kColorNewTabPageSelectedForeground] = {dark_mode ? gfx::kGoogleBlue300
                                                         : gfx::kGoogleBlue700};
  mixer[kColorNewTabPageTagBackground] =
      ui::SetAlpha({dark_mode ? gfx::kGoogleGrey900 : SK_ColorWHITE},
                   /* 90% opacity */ 0.9 * 255);
}
