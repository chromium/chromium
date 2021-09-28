// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/omnibox_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

void AddOmniboxColorMixer(ui::ColorProvider* provider, bool high_contrast) {
  ui::ColorMixer& mixer = provider->AddMixer();
  const float minimum_contrast =
      high_contrast ? 6.0f : color_utils::kMinimumReadableContrastRatio;

  // Omnibox background colors.
  mixer[kColorOmniboxBackground] =
      ui::GetResultingPaintColor(ui::FromTransformInput(), kColorToolbar);
  mixer[kColorOmniboxBackgroundHovered] =
      ui::BlendTowardMaxContrast(kColorOmniboxBackground, 0x0A);

  // Omnibox text colors.
  mixer[kColorOmniboxText] = ui::GetResultingPaintColor(
      ui::FromTransformInput(), kColorOmniboxBackground);
  {
    auto& selected_text = mixer[kColorOmniboxResultsTextSelected];
    selected_text = {kColorOmniboxText};
    if (high_contrast)
      selected_text += ui::ContrastInvert(ui::FromTransformInput());
  }
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
      gfx::kGoogleGreyAlpha300);

  // Results icon colors.
  {
    const auto results_icon = [minimum_contrast](ui::ColorId text_id,
                                                 ui::ColorId background_id) {
      return ui::BlendForMinContrast(ui::DeriveDefaultIconColor(text_id),
                                     background_id, absl::nullopt,
                                     minimum_contrast);
    };
    mixer[kColorOmniboxResultsIcon] =
        results_icon(kColorOmniboxText, kColorOmniboxResultsBackground);
    mixer[kColorOmniboxResultsIconSelected] =
        results_icon(kColorOmniboxResultsTextSelected,
                     kColorOmniboxResultsBackgroundSelected);
  }

  // Dimmed text colors.
  {
    const auto blend_with_clamped_contrast = [minimum_contrast](
                                                 ui::ColorId foreground_id,
                                                 ui::ColorId background_id) {
      return ui::BlendForMinContrast(
          foreground_id, foreground_id,
          ui::BlendForMinContrast(background_id, background_id, absl::nullopt,
                                  minimum_contrast),
          minimum_contrast);
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
    const auto url_color = [minimum_contrast](ui::ColorId id) {
      return ui::BlendForMinContrast(
          gfx::kGoogleBlue500, id,
          ui::SelectBasedOnDarkInput(id, gfx::kGoogleBlue050,
                                     gfx::kGoogleBlue900),
          minimum_contrast);
    };
    mixer[kColorOmniboxResultsUrl] =
        url_color(kColorOmniboxResultsBackgroundHovered);
    mixer[kColorOmniboxResultsUrlSelected] =
        url_color(kColorOmniboxResultsBackgroundSelected);
  }

  // Security chip colors.
  {
    const auto security_chip_color =
        [minimum_contrast](ui::ColorTransform transform) {
          return ui::SelectBasedOnDarkInput(
              kColorOmniboxBackground,
              ui::BlendTowardMaxContrast(kColorOmniboxText, 0x18),
              ui::BlendForMinContrast(std::move(transform),
                                      kColorOmniboxBackgroundHovered,
                                      absl::nullopt, minimum_contrast));
        };
    mixer[kColorOmniboxSecurityChipDangerous] =
        security_chip_color(gfx::kGoogleRed600);
    mixer[kColorOmniboxSecurityChipSecure] =
        security_chip_color(ui::DeriveDefaultIconColor(kColorOmniboxText));
  }
  mixer[kColorOmniboxSecurityChipDefault] = {kColorOmniboxSecurityChipSecure};
}
