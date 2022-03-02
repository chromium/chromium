// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/omnibox_color_mixer.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/buildflags.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

// The contrast for omnibox colors in high contrast mode.
constexpr float kOmniboxHighContrastRatio = 6.0f;

}  // namespace

void AddOmniboxColorMixer(ui::ColorProvider* provider,
                          const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

// Only apply custom high contrast handling on platforms where we are not using
// the system theme for high contrast.
#if BUILDFLAG(USE_GTK) || BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
  const bool high_contrast_custom_handling = false;
#else
  const bool high_contrast_custom_handling =
      key.contrast_mode == ui::ColorProviderManager::ContrastMode::kHigh;
#endif

  const float contrast_ratio = high_contrast_custom_handling
                                   ? kOmniboxHighContrastRatio
                                   : color_utils::kMinimumReadableContrastRatio;
  // kColorOmniboxResultsBackgroundSelected, kColorOmniboxResultsTextSelected,
  // kColorOmniboxResultsTextDimmedSelected, kColorOmniboxResultsIconSelected,
  // and kColorOmniboxResultsUrlSelected will use inverted base colors in high
  // contrast mode.
  const auto selected_background_color =
      high_contrast_custom_handling
          ? ui::ContrastInvert(kColorOmniboxBackground)
          : kColorOmniboxBackground;
  const auto selected_text_color = high_contrast_custom_handling
                                       ? ui::ContrastInvert(kColorOmniboxText)
                                       : kColorOmniboxText;

  // Omnibox background colors.
  mixer[kColorOmniboxBackground] =
      ui::GetResultingPaintColor(ui::FromTransformInput(), kColorToolbar);
  mixer[kColorOmniboxBackgroundHovered] =
      ui::BlendTowardMaxContrast(kColorOmniboxBackground, 0x0A);

  // Omnibox text colors.
  mixer[kColorOmniboxText] = ui::GetResultingPaintColor(
      ui::FromTransformInput(), kColorOmniboxBackground);
  mixer[kColorOmniboxResultsTextSelected] = {selected_text_color};
  mixer[kColorOmniboxKeywordSelected] = ui::SelectBasedOnDarkInput(
      kColorOmniboxBackground, gfx::kGoogleGrey100, kColorOmniboxResultsUrl);

  // Bubble outline colors.
  mixer[kColorOmniboxBubbleOutline] =
      ui::SelectBasedOnDarkInput(kColorOmniboxBackground, gfx::kGoogleGrey100,
                                 SkColorSetA(gfx::kGoogleGrey900, 0x24));
  mixer[kColorOmniboxBubbleOutlineExperimentalKeywordMode] = {
      kColorOmniboxKeywordSelected};

  // Results background colors.
  mixer[kColorOmniboxResultsBackground] =
      ui::GetColorWithMaxContrast(kColorOmniboxText);
  mixer[kColorOmniboxResultsBackgroundHovered] = ui::BlendTowardMaxContrast(
      kColorOmniboxResultsBackground, gfx::kGoogleGreyAlpha200);
  mixer[kColorOmniboxResultsBackgroundSelected] = ui::BlendTowardMaxContrast(
      ui::GetColorWithMaxContrast(kColorOmniboxResultsTextSelected),
      gfx::kGoogleGreyAlpha200);

  // Results icon colors.
  {
    const auto results_icon = [contrast_ratio](ui::ColorId text_id,
                                               ui::ColorId background_id) {
      return ui::BlendForMinContrast(ui::DeriveDefaultIconColor(text_id),
                                     background_id, absl::nullopt,
                                     contrast_ratio);
    };
    mixer[kColorOmniboxResultsIcon] =
        results_icon(kColorOmniboxText, kColorOmniboxResultsBackground);
    mixer[kColorOmniboxResultsIconSelected] =
        results_icon(kColorOmniboxResultsTextSelected,
                     kColorOmniboxResultsBackgroundSelected);
  }

  // Dimmed text colors.
  {
    const auto blend_with_clamped_contrast =
        [contrast_ratio](ui::ColorId foreground_id, ui::ColorId background_id) {
          return ui::BlendForMinContrast(
              foreground_id, foreground_id,
              ui::BlendForMinContrast(background_id, background_id,
                                      absl::nullopt, contrast_ratio),
              contrast_ratio);
        };
    mixer[kColorOmniboxResultsTextDimmed] = blend_with_clamped_contrast(
        kColorOmniboxText, kColorOmniboxResultsBackgroundHovered);
    mixer[kColorOmniboxResultsTextDimmedSelected] =
        blend_with_clamped_contrast(kColorOmniboxResultsTextSelected,
                                    kColorOmniboxResultsBackgroundSelected);
    mixer[kColorOmniboxTextDimmed] = blend_with_clamped_contrast(
        kColorOmniboxText, kColorOmniboxBackgroundHovered);
  }

  // Results URL colors.
  {
    const auto url_color = [contrast_ratio](ui::ColorId id,
                                            ui::ColorTransform background) {
      return ui::BlendForMinContrast(
          gfx::kGoogleBlue500, id,
          ui::SelectBasedOnDarkInput(background, gfx::kGoogleBlue050,
                                     gfx::kGoogleBlue900),
          contrast_ratio);
    };

    mixer[kColorOmniboxResultsUrl] = url_color(
        kColorOmniboxResultsBackgroundHovered, kColorOmniboxBackground);
    mixer[kColorOmniboxResultsUrlSelected] = url_color(
        kColorOmniboxResultsBackgroundSelected, selected_background_color);
  }

  // Security chip colors.
  {
    const auto security_chip_color = [contrast_ratio](SkColor dark_input,
                                                      SkColor light_input) {
      return ui::BlendForMinContrast(
          ui::SelectBasedOnDarkInput(kColorOmniboxBackground, dark_input,
                                     light_input),
          kColorOmniboxBackgroundHovered, absl::nullopt, contrast_ratio);
    };

    mixer[kColorOmniboxSecurityChipDangerous] =
        security_chip_color(gfx::kGoogleRed300, gfx::kGoogleRed600);
    // TODO(weili): consider directly deriving from the omnibox text color such
    // as using
    // security_chip_color(ui::DeriveDefaultIconColor(kColorOmniboxText)).
    mixer[kColorOmniboxSecurityChipSecure] =
        security_chip_color(gfx::kGoogleGrey500, gfx::kGoogleGrey700);
    mixer[kColorOmniboxSecurityChipDefault] = {kColorOmniboxSecurityChipSecure};
  }
}
