// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/omnibox_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "third_party/skia/include/core/SkColor.h"
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

  const bool high_contrast_custom_handling = ShouldApplyHighContrastColors(key);
  const float contrast_ratio = high_contrast_custom_handling
                                   ? kOmniboxHighContrastRatio
                                   : color_utils::kMinimumReadableContrastRatio;
  // Selected colors will use inverted base colors in high contrast mode.
  const auto selected_background_color =
      high_contrast_custom_handling
          ? ui::ContrastInvert(kColorToolbarBackgroundSubtleEmphasis)
          : kColorToolbarBackgroundSubtleEmphasis;
  const auto selected_text_color = high_contrast_custom_handling
                                       ? ui::ContrastInvert(kColorOmniboxText)
                                       : kColorOmniboxText;

  // Location bar colors.
  mixer[kColorLocationBarClearAllButtonIcon] =
      ui::DeriveDefaultIconColor(kColorOmniboxText);
  mixer[kColorLocationBarClearAllButtonIconDisabled] = ui::SetAlpha(
      kColorLocationBarClearAllButtonIcon, gfx::kDisabledControlAlpha);

  // Omnibox background colors.
  mixer[kColorToolbarBackgroundSubtleEmphasis] = ui::SelectBasedOnDarkInput(
      kColorToolbar, gfx::kGoogleGrey900, gfx::kGoogleGrey100);
  mixer[kColorToolbarBackgroundSubtleEmphasisHovered] =
      ui::BlendTowardMaxContrast(kColorToolbarBackgroundSubtleEmphasis, 0x0A);

  // Omnibox text colors.
  mixer[kColorOmniboxText] =
      ui::GetColorWithMaxContrast(kColorToolbarBackgroundSubtleEmphasis);
  mixer[kColorOmniboxResultsTextSelected] = {selected_text_color};
  mixer[kColorOmniboxKeywordSelected] =
      ui::SelectBasedOnDarkInput(kColorToolbarBackgroundSubtleEmphasis,
                                 gfx::kGoogleGrey100, kColorOmniboxResultsUrl);

  // Bubble outline colors.
  mixer[kColorOmniboxBubbleOutline] = ui::SelectBasedOnDarkInput(
      kColorToolbarBackgroundSubtleEmphasis, gfx::kGoogleGrey100,
      SkColorSetA(gfx::kGoogleGrey900, 0x24));
  mixer[kColorOmniboxBubbleOutlineExperimentalKeywordMode] = {
      kColorOmniboxKeywordSelected};

  // Results background, button, and focus colors.
  mixer[kColorOmniboxResultsBackground] =
      ui::GetColorWithMaxContrast(kColorOmniboxText);
  mixer[kColorOmniboxResultsBackgroundHovered] = ui::BlendTowardMaxContrast(
      kColorOmniboxResultsBackground, gfx::kGoogleGreyAlpha200);
  mixer[kColorOmniboxResultsBackgroundSelected] = ui::BlendTowardMaxContrast(
      ui::GetColorWithMaxContrast(kColorOmniboxResultsTextSelected),
      gfx::kGoogleGreyAlpha200);
  mixer[kColorOmniboxResultsButtonBorder] = ui::BlendTowardMaxContrast(
      kColorToolbarBackgroundSubtleEmphasis, gfx::kGoogleGreyAlpha400);
  mixer[kColorOmniboxResultsButtonInkDrop] =
      ui::GetColorWithMaxContrast(kColorOmniboxResultsBackgroundHovered);
  mixer[kColorOmniboxResultsButtonInkDropSelected] =
      ui::GetColorWithMaxContrast(kColorOmniboxResultsBackgroundSelected);
  mixer[kColorOmniboxResultsFocusIndicator] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, kColorOmniboxResultsBackgroundSelected,
      color_utils::kMinimumVisibleContrastRatio);

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
    mixer[kColorOmniboxResultsStarterPackIcon] = ui::BlendForMinContrast(
        gfx::kGoogleBlue600, kColorOmniboxResultsBackground, absl::nullopt,
        color_utils::kMinimumVisibleContrastRatio);
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
        kColorOmniboxText, kColorToolbarBackgroundSubtleEmphasisHovered);
  }

  // Other results text colors.
  {
    const auto negative_color = [contrast_ratio](
                                    ui::ColorId background,
                                    ui::ColorTransform dark_selector) {
      return ui::BlendForMinContrast(
          // Like kColorAlertHighSeverity, but toggled on `dark_selector`.
          ui::SelectBasedOnDarkInput(dark_selector, gfx::kGoogleRed300,
                                     gfx::kGoogleRed600),
          background, absl::nullopt, contrast_ratio);
    };
    const auto positive_color = [contrast_ratio](
                                    ui::ColorId background,
                                    ui::ColorTransform dark_selector) {
      return ui::BlendForMinContrast(
          // Like kColorAlertLowSeverity, but toggled on `dark_selector`.
          ui::SelectBasedOnDarkInput(dark_selector, gfx::kGoogleGreen300,
                                     gfx::kGoogleGreen700),
          background, absl::nullopt, contrast_ratio);
    };
    const auto secondary_color = [contrast_ratio](
                                     ui::ColorId background,
                                     ui::ColorTransform dark_selector) {
      return ui::BlendForMinContrast(
          // Like kColorDisabledForeground, but toggled on `dark_selector`.
          ui::BlendForMinContrast(
              gfx::kGoogleGrey600,
              ui::SelectBasedOnDarkInput(dark_selector,
                                         SkColorSetRGB(0x29, 0x2A, 0x2D),
                                         SK_ColorWHITE),
              ui::SelectBasedOnDarkInput(dark_selector, gfx::kGoogleGrey200,
                                         gfx::kGoogleGrey900)),
          background, absl::nullopt, contrast_ratio);
    };
    const auto url_color = [contrast_ratio](ui::ColorId background,
                                            ui::ColorTransform dark_selector) {
      return ui::BlendForMinContrast(
          gfx::kGoogleBlue500, background,
          ui::SelectBasedOnDarkInput(dark_selector, gfx::kGoogleBlue050,
                                     gfx::kGoogleBlue900),
          contrast_ratio);
    };

    mixer[kColorOmniboxResultsTextNegative] =
        negative_color(kColorOmniboxResultsBackgroundHovered,
                       kColorToolbarBackgroundSubtleEmphasis);
    mixer[kColorOmniboxResultsTextNegativeSelected] = negative_color(
        kColorOmniboxResultsBackgroundSelected, selected_background_color);
    mixer[kColorOmniboxResultsTextPositive] =
        positive_color(kColorOmniboxResultsBackgroundHovered,
                       kColorToolbarBackgroundSubtleEmphasis);
    mixer[kColorOmniboxResultsTextPositiveSelected] = positive_color(
        kColorOmniboxResultsBackgroundSelected, selected_background_color);
    mixer[kColorOmniboxResultsTextSecondary] =
        secondary_color(kColorOmniboxResultsBackgroundHovered,
                        kColorToolbarBackgroundSubtleEmphasis);
    mixer[kColorOmniboxResultsTextSecondarySelected] = secondary_color(
        kColorOmniboxResultsBackgroundSelected, selected_background_color);
    mixer[kColorOmniboxResultsUrl] =
        url_color(kColorOmniboxResultsBackgroundHovered,
                  kColorToolbarBackgroundSubtleEmphasis);
    mixer[kColorOmniboxResultsUrlSelected] = url_color(
        kColorOmniboxResultsBackgroundSelected, selected_background_color);
  }

  // Security chip colors.
  {
    const auto security_chip_color = [contrast_ratio](SkColor dark_input,
                                                      SkColor light_input) {
      return ui::BlendForMinContrast(
          ui::SelectBasedOnDarkInput(kColorToolbarBackgroundSubtleEmphasis,
                                     dark_input, light_input),
          kColorToolbarBackgroundSubtleEmphasisHovered, absl::nullopt,
          contrast_ratio);
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

  // TODO(manukh): Figure out if we can use the blending defined above and in
  //   `ui::` instead of hard coding these colors. That'll probably be safer for
  //   e.g. when users use high contrast mode. But this is (hopefully) fine for
  //   non-launch experiments.
  mixer[kColorOmniboxResultsIconGM3Background] = ui::SelectBasedOnDarkInput(
      kColorToolbar, SkColorSetRGB(48, 48, 48), SkColorSetRGB(242, 242, 242));
  mixer[kColorOmniboxAnswerIconGM3Background] = ui::SelectBasedOnDarkInput(
      kColorToolbar, SkColorSetRGB(0, 74, 119), SkColorSetRGB(211, 227, 253));
  mixer[kColorOmniboxAnswerIconGM3Foreground] = ui::SelectBasedOnDarkInput(
      kColorToolbar, SkColorSetRGB(194, 231, 255), SkColorSetRGB(4, 30, 73));
}
