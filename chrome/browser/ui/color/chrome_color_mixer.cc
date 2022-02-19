// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixer.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
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

ui::ColorTransform AdjustHighlightColorForContrast(
    ui::ColorTransform background,
    ui::ColorTransform desired_dark,
    ui::ColorTransform desired_light,
    ui::ColorTransform dark_extreme,
    ui::ColorTransform light_extreme) {
  const auto generator = [](ui::ColorTransform background,
                            ui::ColorTransform desired_dark,
                            ui::ColorTransform desired_light,
                            ui::ColorTransform dark_extreme,
                            ui::ColorTransform light_extreme,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor background_color = background.Run(input_color, mixer);
    const SkColor desired_dark_color = desired_dark.Run(input_color, mixer);
    const SkColor desired_light_color = desired_light.Run(input_color, mixer);
    const SkColor dark_extreme_color = dark_extreme.Run(input_color, mixer);
    const SkColor light_extreme_color = light_extreme.Run(input_color, mixer);
    const SkColor contrasting_color = color_utils::PickContrastingColor(
        desired_dark_color, desired_light_color, background_color);
    const SkColor limit_color = contrasting_color == desired_dark_color
                                    ? dark_extreme_color
                                    : light_extreme_color;
    // Setting highlight color will set the text to the highlight color, and the
    // background to the same color with a low alpha. This means that our target
    // contrast is between the text (the highlight color) and a blend of the
    // highlight color and the toolbar color.
    const SkColor base_color = color_utils::AlphaBlend(
        contrasting_color, background_color, SkAlpha{0x20});

    // Add a fudge factor to the minimum contrast ratio since we'll actually be
    // blending with the adjusted color.
    const SkColor result_color =
        color_utils::BlendForMinContrast(
            contrasting_color, base_color, limit_color,
            color_utils::kMinimumReadableContrastRatio * 1.05)
            .color;
    DVLOG(2) << "ColorTransform AdjustHighlightColorForContrast:"
             << " Background: " << ui::SkColorName(background_color)
             << " Desired Dark: " << ui::SkColorName(desired_dark_color)
             << " Desired Light: " << ui::SkColorName(desired_light_color)
             << " Dark Extreme: " << ui::SkColorName(dark_extreme_color)
             << " Light Extreme: " << ui::SkColorName(light_extreme_color)
             << " Contrasting Color: " << ui::SkColorName(contrasting_color)
             << " Limit Color: " << ui::SkColorName(limit_color)
             << " Base Color: " << ui::SkColorName(base_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(background),
                             std::move(desired_dark), std::move(desired_light),
                             std::move(dark_extreme), std::move(light_extreme));
}

}  // namespace

void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorAppMenuHighlightSeverityLow] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleGreen600, gfx::kGoogleGreen300,
      gfx::kGoogleGreen900, gfx::kGoogleGreen050);
  mixer[kColorAppMenuHighlightSeverityHigh] = {
      kColorAvatarButtonHighlightSyncError};
  mixer[kColorAppMenuHighlightSeverityMedium] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleYellow600,
      gfx::kGoogleYellow300, gfx::kGoogleYellow900, gfx::kGoogleYellow050);
  mixer[kColorAvatarButtonHighlightNormal] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleBlue600, gfx::kGoogleBlue300,
      gfx::kGoogleBlue900, gfx::kGoogleBlue050);
  mixer[kColorAvatarButtonHighlightSyncError] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleRed600, gfx::kGoogleRed300,
      gfx::kGoogleRed900, gfx::kGoogleRed050);
  mixer[kColorAvatarButtonHighlightSyncPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorBookmarkBarBackground] = {kColorToolbar};
  mixer[kColorBookmarkBarForeground] = {kColorToolbarText};
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIcon};
  // If the custom theme supplies a specific color for the bookmark text, use
  // that color to derive folder icon color. We don't actually use the color
  // returned, rather we use the color provider color transform corresponding to
  // that color.
  SkColor color;
  const bool custom_icon_color =
      key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON,
                                 &color);
  mixer[kColorBookmarkFavicon] =
      custom_icon_color ? ui::ColorTransform(kColorToolbarButtonIcon)
                        : ui::ColorTransform(SK_ColorTRANSPARENT);
  const bool custom_text_color =
      key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT, &color);
  mixer[kColorBookmarkFolderIcon] =
      custom_text_color
          ? ui::DeriveDefaultIconColor(kColorBookmarkBarForeground)
          : ui::ColorTransform(ui::kColorIcon);
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparator};
  mixer[kColorDownloadShelf] = {kColorToolbar};
  mixer[kColorDownloadShelfButtonBackground] = {kColorDownloadShelf};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorAccent, kColorDownloadShelf,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadToolbarButtonActive] = {ui::kColorThrobber};
  mixer[kColorDownloadToolbarButtonInactive] = {ui::kColorMidground};
  mixer[kColorDownloadToolbarButtonRingBackground] = {
      SkColorSetA(kColorDownloadToolbarButtonInactive, 0x33)};
  mixer[kColorFrameCaptionActive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameActive});
  mixer[kColorFrameCaptionInactive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameInactive});
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  mixer[kColorGooglePayLogo] = {dark_mode ? SK_ColorWHITE
                                          : gfx::kGoogleGrey700};
#endif
  mixer[kColorLocationBarBorder] = {SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorNewTabPageBackground] = {kColorToolbar};
  mixer[kColorNewTabPageHeader] = {SkColorSetRGB(0x96, 0x96, 0x96)};
  mixer[kColorNewTabPageText] = {dark_mode ? gfx::kGoogleGrey200
                                           : SK_ColorBLACK};
  mixer[kColorOmniboxBackground] = {dark_mode ? gfx::kGoogleGrey900
                                              : gfx::kGoogleGrey100};
  mixer[kColorOmniboxText] =
      ui::GetColorWithMaxContrast(kColorOmniboxBackground);
  mixer[kColorReadLaterButtonHighlight] = {kColorAvatarButtonHighlightNormal};
  mixer[kColorTabGroupContextMenuBlue] = {dark_mode ? gfx::kGoogleBlue300
                                                    : gfx::kGoogleBlue600};
  mixer[kColorTabGroupContextMenuCyan] = {dark_mode ? gfx::kGoogleCyan300
                                                    : gfx::kGoogleCyan900};
  mixer[kColorTabGroupContextMenuGreen] = {dark_mode ? gfx::kGoogleGreen300
                                                     : gfx::kGoogleGreen700};
  mixer[kColorTabGroupContextMenuGrey] = {dark_mode ? gfx::kGoogleGrey300
                                                    : gfx::kGoogleGrey700};
  mixer[kColorTabGroupContextMenuOrange] = {dark_mode ? gfx::kGoogleOrange300
                                                      : gfx::kGoogleOrange400};
  mixer[kColorTabGroupContextMenuPink] = {dark_mode ? gfx::kGooglePink300
                                                    : gfx::kGooglePink700};
  mixer[kColorTabGroupContextMenuPurple] = {dark_mode ? gfx::kGooglePurple300
                                                      : gfx::kGooglePurple500};
  mixer[kColorTabGroupContextMenuRed] = {dark_mode ? gfx::kGoogleRed300
                                                   : gfx::kGoogleRed600};
  mixer[kColorTabGroupContextMenuYellow] = {dark_mode ? gfx::kGoogleYellow300
                                                      : gfx::kGoogleYellow600};
  mixer[kColorToolbar] = {dark_mode ? SkColorSetRGB(0x35, 0x36, 0x3A)
                                    : SK_ColorWHITE};
  mixer[kColorToolbarButtonBackground] =
      ui::GetColorWithMaxContrast(kColorToolbarButtonText);
  mixer[kColorToolbarButtonBorder] = ui::SetAlpha(kColorToolbarInkDrop, 0x20);
  mixer[kColorToolbarButtonIcon] = ui::HSLShift(
      gfx::kChromeIconGrey, GetThemeTint(ThemeProperties::TINT_BUTTONS, key));
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kGoogleGreyAlpha500)};
  mixer[kColorToolbarButtonIconPressed] = {kColorToolbarButtonIconHovered};
  // TODO(crbug.com/967317): Update to match mocks, i.e. return
  // gfx::kGoogleGrey900, if needed.
  mixer[kColorToolbarButtonText] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarContentAreaSeparator] =
      ui::SetAlpha(kColorToolbarButtonIcon, 0x3A);
  mixer[kColorToolbarFeaturePromoHighlight] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleBlue600, gfx::kGoogleGrey100,
      gfx::kGoogleBlue900, SK_ColorWHITE);
  mixer[kColorToolbarInkDrop] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarSeparator] = ui::SetAlpha(kColorToolbarButtonIcon, 0x4D);
  mixer[kColorToolbarText] = {dark_mode ? SK_ColorWHITE : gfx::kGoogleGrey800};
}
