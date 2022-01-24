// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/omnibox_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

void AddOmniboxColorMixer(ui::ColorProvider* provider,
                          const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // Omnibox background colors.
  mixer[kColorOmniboxBackground] =
      ui::GetResultingPaintColor(ui::FromTransformInput(), kColorToolbar);
  mixer[kColorOmniboxBackgroundHovered] =
      ui::BlendTowardMaxContrast(kColorOmniboxBackground, 0x0A);

  // Omnibox text colors.
  mixer[kColorOmniboxText] = ui::GetResultingPaintColor(
      ui::FromTransformInput(), kColorOmniboxBackground);
  mixer[kColorOmniboxResultsTextSelected] = {kColorOmniboxText};
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
    const auto results_icon = [](ui::ColorId text_id,
                                 ui::ColorId background_id) {
      return ui::BlendForMinContrast(ui::DeriveDefaultIconColor(text_id),
                                     background_id);
    };
    mixer[kColorOmniboxResultsIcon] =
        results_icon(kColorOmniboxText, kColorOmniboxResultsBackground);
    mixer[kColorOmniboxResultsIconSelected] =
        results_icon(kColorOmniboxResultsTextSelected,
                     kColorOmniboxResultsBackgroundSelected);
  }

  // Dimmed text colors.
  {
    const auto blend_with_clamped_contrast = [](ui::ColorId foreground_id,
                                                ui::ColorId background_id) {
      return ui::BlendForMinContrast(
          foreground_id, foreground_id,
          ui::BlendForMinContrast(background_id, background_id));
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
    const auto url_color = [](ui::ColorId id) {
      return ui::BlendForMinContrast(
          gfx::kGoogleBlue500, id,
          ui::SelectBasedOnDarkInput(kColorOmniboxBackground,
                                     gfx::kGoogleBlue050, gfx::kGoogleBlue900));
    };
    mixer[kColorOmniboxResultsUrl] =
        url_color(kColorOmniboxResultsBackgroundHovered);
    mixer[kColorOmniboxResultsUrlSelected] =
        url_color(kColorOmniboxResultsBackgroundSelected);
  }

  // Security chip colors.
  {
    const auto security_chip_color = [](SkColor dark_input,
                                        SkColor light_input) {
      return ui::BlendForMinContrast(
          ui::SelectBasedOnDarkInput(kColorOmniboxBackground, dark_input,
                                     light_input),
          kColorOmniboxBackgroundHovered);
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
