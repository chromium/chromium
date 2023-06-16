// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/omnibox_color_mixer.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "components/omnibox/common/omnibox_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
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

// Apply updates to the Omnibox text color tokens per GM3 spec.
void ApplyGM3OmniboxTextColor(ui::ColorMixer& mixer,
                              const ui::ColorProviderManager::Key& key) {
  const bool gm3_text_color_enabled =
      features::GetChromeRefresh2023Level() ==
          features::ChromeRefresh2023Level::kLevel2 ||
      base::FeatureList::IsEnabled(omnibox::kOmniboxSteadyStateTextColor);

  if (!gm3_text_color_enabled) {
    return;
  }

  // Retrieve GM3 omnibox text color params (Dark Mode).
  const std::string dark_text_color_param =
      omnibox::kOmniboxTextColorDarkMode.Get();
  const std::string dark_text_color_dimmed_param =
      omnibox::kOmniboxTextColorDimmedDarkMode.Get();

  // Retrieve GM3 omnibox text color params (Light Mode).
  const std::string light_text_color_param =
      omnibox::kOmniboxTextColorLightMode.Get();
  const std::string light_text_color_dimmed_param =
      omnibox::kOmniboxTextColorDimmedLightMode.Get();

  const auto string_to_skcolor = [](const std::string& rgb_str,
                                    SkColor* result) {
    // Valid color strings are of the form 0xRRGGBB or 0xAARRGGBB.
    const bool valid = result && (rgb_str.size() == 8 || rgb_str.size() == 10);
    if (!valid) {
      return false;
    }

    uint32_t parsed = 0;
    const bool success = base::HexStringToUInt(rgb_str, &parsed);
    if (success) {
      *result = SkColorSetA(static_cast<SkColor>(parsed), SK_AlphaOPAQUE);
    }
    return success;
  };

  SkColor dark_text_color = 0;
  SkColor dark_text_color_dimmed = 0;

  SkColor light_text_color = 0;
  SkColor light_text_color_dimmed = 0;

  const bool success =
      string_to_skcolor(dark_text_color_param, &dark_text_color) &&
      string_to_skcolor(dark_text_color_dimmed_param,
                        &dark_text_color_dimmed) &&
      string_to_skcolor(light_text_color_param, &light_text_color) &&
      string_to_skcolor(light_text_color_dimmed_param,
                        &light_text_color_dimmed);

  if (!success) {
    return;
  }

  const auto selected_text_color = ui::SelectBasedOnDarkInput(
      kColorToolbar, dark_text_color, light_text_color);

  mixer[kColorOmniboxText] = {selected_text_color};

  const auto selected_text_color_dimmed = ui::SelectBasedOnDarkInput(
      kColorToolbar, dark_text_color_dimmed, light_text_color_dimmed);

  mixer[kColorOmniboxTextDimmed] = {selected_text_color_dimmed};
}

void ApplyCR2023OmniboxIconColors(ui::ColorMixer& mixer,
                                  const ui::ColorProviderManager::Key& key) {
  const bool cr2023_icons_colors_enabled =
      features::GetChromeRefresh2023Level() ==
          features::ChromeRefresh2023Level::kLevel2 ||
      base::FeatureList::IsEnabled(omnibox::kOmniboxCR23SteadyStateIcons);

  if (!cr2023_icons_colors_enabled) {
    return;
  }

  mixer[kColorPageActionIconHover] = {ui::kColorSysStateHoverOnSubtle};
  mixer[kColorPageActionIconPressed] = {
      ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorPageInfoBackground] = {ui::kColorSysBaseContainerElevated};
  mixer[kColorPageInfoIconHover] = {ui::kColorSysStateHoverDimBlendProtection};
  mixer[kColorPageInfoIconPressed] = {ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorPageActionIcon] = {ui::kColorSysOnSurfaceSubtle};
}

// Apply updates to the Omnibox "expanded state" color tokens per CR2023 spec.
void ApplyCR2023OmniboxExpandedStateColors(
    ui::ColorMixer& mixer,
    const ui::ColorProviderManager::Key& key) {
  const bool cr2023_expanded_state_colors_enabled =
      features::GetChromeRefresh2023Level() ==
          features::ChromeRefresh2023Level::kLevel2 ||
      base::FeatureList::IsEnabled(omnibox::kExpandedStateColors);

  if (!cr2023_expanded_state_colors_enabled) {
    return;
  }

  // Update focus bar color.
  mixer[kColorOmniboxResultsFocusIndicator] = {ui::kColorSysStateFocusRing};

  // Update omnibox popup background color.
  mixer[kColorOmniboxResultsBackground] = {ui::kColorSysBase};

  // Update suggestion hover fill colors.
  mixer[kColorOmniboxResultsBackgroundHovered] = ui::SelectBasedOnDarkInput(
      kColorOmniboxResultsBackground,
      SkColorSetA(static_cast<SkColor>(0x4F4F4F), SK_AlphaOPAQUE),
      ui::kColorSysInverseOnSurface);
  mixer[kColorOmniboxResultsBackgroundSelected] = {
      kColorOmniboxResultsBackgroundHovered};

  // Update URL link color.
  mixer[kColorOmniboxResultsUrl] = {ui::kColorSysPrimary};

  // Update keyword mode icon & text color.
  mixer[kColorOmniboxKeywordSelected] = {kColorOmniboxResultsUrl};

  // Update keyword mode separator color.
  mixer[kColorOmniboxKeywordSeparator] = {ui::kColorSysTonalOutline};

  // Update suggest text and separator dim color.
  mixer[kColorOmniboxResultsTextDimmed] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorOmniboxResultsTextDimmedSelected] = {
      kColorOmniboxResultsTextDimmed};

  // Update suggestion vector icon color.
  mixer[kColorOmniboxResultsIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorOmniboxResultsIconSelected] = {kColorOmniboxResultsIcon};

  // Update chip colors.
  mixer[kColorOmniboxResultsButtonBorder] = {kColorOmniboxKeywordSeparator};
  mixer[kColorOmniboxResultsButtonIcon] = {kColorOmniboxResultsUrl};
  mixer[kColorOmniboxResultsButtonIconSelected] = {
      kColorOmniboxResultsButtonIcon};
  // TODO(crbug.com/1431337) Update to use sys tokens. We need a sys token like
  //   `{dark_mode ? kColorRefNeutral90 : kColorRefNeutral65}`.
  mixer[kColorOmniboxResultsButtonInkDrop] =
      ui::SelectBasedOnDarkInput(kColorToolbar, SkColorSetRGB(226, 226, 226),
                                 SkColorSetRGB(153, 153, 153));
  mixer[kColorOmniboxResultsButtonInkDropSelected] = {
      kColorOmniboxResultsButtonInkDrop};

  // Update starter pack icon color.
  mixer[kColorOmniboxResultsStarterPackIcon] = {ui::kColorSysPrimary};
}

// Apply updates to the Omnibox color tokens per CR2023 guidelines.
void ApplyOmniboxCR2023Colors(ui::ColorMixer& mixer,
                              const ui::ColorProviderManager::Key& key) {
  // Do not apply CR2023 Omnibox colors to clients using high-contrast
  // mode or a custom theme.
  // TODO(khalidpeer): Roll out CR2023 color updates for high-contrast clients.
  // TODO(khalidpeer): Roll out CR2023 color updates for themed clients.
  if (ShouldApplyHighContrastColors(key) || key.custom_theme) {
    return;
  }
  ApplyGM3OmniboxTextColor(mixer, key);
  ApplyCR2023OmniboxExpandedStateColors(mixer, key);
  ApplyCR2023OmniboxIconColors(mixer, key);
}

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
  mixer[kColorOmniboxKeywordSeparator] = {kColorOmniboxText};

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
  mixer[kColorOmniboxResultsButtonIcon] = {kColorOmniboxResultsIcon};
  mixer[kColorOmniboxResultsButtonIconSelected] = {
      kColorOmniboxResultsIconSelected};
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

  // location bar icon colors.
  mixer[kColorPageInfoBackground] = {kColorToolbar};
  // Literal constants are `kOmniboxOpacityHovered` and
  // `kOmniboxOpacitySelected`. This is so that we can more cleanly use the
  // colors in the inkdrop instead of handling themes and non-themes separately
  // in-code as they have different opacity requirements.
  mixer[kColorPageInfoIconHover] = {
      ui::SetAlpha(kColorOmniboxText, std::ceil(0.10f * 255.0f))};
  mixer[kColorPageInfoIconPressed] = {
      ui::SetAlpha(kColorOmniboxText, std::ceil(0.16f * 255.0f))};
  mixer[kColorPageActionIconHover] = {kColorPageInfoIconHover};
  mixer[kColorPageActionIconPressed] = {kColorPageInfoIconPressed};
  mixer[kColorPageActionIcon] = {kColorOmniboxResultsIcon};

  // Override omnibox colors per CR2023 spec.
  ApplyOmniboxCR2023Colors(mixer, key);
}
