// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/omnibox_color_mixer.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

// The contrast for omnibox colors in high contrast mode.
constexpr float kOmniboxHighContrastRatio = 6.0f;

// Apply updates to the Omnibox text color tokens per GM3 spec.
void ApplyGM3OmniboxTextColor(ui::ColorMixer& mixer,
                              const ui::ColorProviderKey& key) {
  mixer[kColorOmniboxText] = {ui::kColorSysOnSurface};
  mixer[kColorOmniboxTextDimmed] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorOmniboxSelectionBackground] = {ui::kColorSysStateTextHighlight};
  mixer[kColorOmniboxSelectionForeground] = {ui::kColorSysStateOnTextHighlight};

  // In high-contrast mode, text colors have selected variants. This is because
  // the selected suggestion has a high-contrast background, so when the
  // unselected text needs to be near-white, the selected text needs to be
  // near-black (or vice versa). Though there are bugs where some of the views
  // apply the selected variants to the 1st suggestion instead of the selected
  // suggestion. Regardless,for now, CR23 does not apply in high-contrast mode,
  // so it's safe to use the unselected colors.
  // TODO(manukh): Figure out correct colors when launching CR23 for
  //   high-contrast.
  mixer[kColorOmniboxResultsTextSelected] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsUrlSelected] = {kColorOmniboxResultsUrl};

  // These affect finance answers; e.g. 'goog stock'.
  // TODO(crbug.com/40923750): These don't seem to apply anymore, at least on
  //   desktop. Check with UX if we still care to color finance answers, and
  //   what those colors should in CR23.
  mixer[kColorOmniboxResultsTextNegativeSelected] = {
      kColorOmniboxResultsTextNegative};
  mixer[kColorOmniboxResultsTextPositiveSelected] = {
      kColorOmniboxResultsTextPositive};
  mixer[kColorOmniboxResultsTextSecondarySelected] = {
      kColorOmniboxResultsTextSecondary};
}

void ApplyCR2023OmniboxIconColors(ui::ColorMixer& mixer,
                                  const ui::ColorProviderKey& key) {
  mixer[kColorPageActionIconHover] = {ui::kColorSysStateHoverOnSubtle};
  mixer[kColorPageInfoBackground] = {ui::kColorSysBaseContainerElevated};
  mixer[kColorPageInfoBackgroundTonal] = {ui::kColorSysTonalContainer};
  mixer[kColorPageInfoForeground] = {ui::kColorSysOnSurface};
  mixer[kColorPageInfoSubtitleForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorPageInfoForegroundTonal] = {ui::kColorSysOnTonalContainer};
  mixer[kColorPageInfoIconHover] = {ui::kColorSysStateHoverDimBlendProtection};
  mixer[kColorPageInfoIconPressed] = {ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorPageActionIcon] = {ui::kColorSysOnSurfaceSubtle};

  // Security chip.
  mixer[kColorOmniboxSecurityChipDangerousBackground] = {ui::kColorSysError};
  mixer[kColorOmniboxSecurityChipText] = {ui::kColorSysOnError};
  mixer[kColorOmniboxSecurityChipInkDropHover] = {
      ui::kColorSysStateHoverOnProminent};
  mixer[kColorOmniboxSecurityChipInkDropRipple] = {
      ui::kColorSysStateRippleNeutralOnProminent};
}

// Apply updates to the Omnibox "expanded state" color tokens per CR2023 spec.
void ApplyCR2023OmniboxExpandedStateColors(ui::ColorMixer& mixer,
                                           const ui::ColorProviderKey& key) {
  // Update focus bar color.
  mixer[kColorOmniboxResultsFocusIndicator] = {ui::kColorSysStateFocusRing};

  // Update omnibox popup background color.
  mixer[kColorOmniboxResultsBackground] = {ui::kColorSysBase};

  // Update suggestion hover fill colors.
  mixer[kColorOmniboxResultsBackgroundHovered] = ui::GetResultingPaintColor(
      ui::kColorSysStateHoverOnSubtle, kColorOmniboxResultsBackground);
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
  // Action chip hover & select colors for hovered suggestion rows (e.g. via
  // mouse cursor).
  mixer[kColorOmniboxResultsButtonInkDropRowHovered] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorOmniboxResultsButtonInkDropSelectedRowHovered] = {
      ui::kColorSysStateRippleNeutralOnSubtle};
  // Action chip hover & select colors for selected suggestion rows (e.g. via
  // arrow keys).
  mixer[kColorOmniboxResultsButtonInkDropRowSelected] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorOmniboxResultsButtonInkDropSelectedRowSelected] = {
      ui::kColorSysStateRippleNeutralOnSubtle};

  // Update starter pack icon color.
  mixer[kColorOmniboxResultsStarterPackIcon] = {ui::kColorSysOnTonalContainer};
}

// Apply fallback Omnibox color mappings for CR2023 clients who are not eligible
// for the usual color treatment (due to using high-contrast mode and/or a
// custom theme).
void ApplyOmniboxCR2023FallbackColors(ui::ColorMixer& mixer,
                                      const ui::ColorProviderKey& key) {
  // Action chip hover & select colors for hovered suggestion rows (e.g. via
  // mouse cursor).
  mixer[kColorOmniboxResultsButtonInkDropRowHovered] = {ui::SetAlpha(
      kColorOmniboxResultsButtonInkDrop, std::ceil(0.10f * 255.0f))};
  mixer[kColorOmniboxResultsButtonInkDropSelectedRowHovered] = {ui::SetAlpha(
      kColorOmniboxResultsButtonInkDrop, std::ceil(0.16f * 255.0f))};
  // Action chip hover & select colors for selected suggestion rows (e.g. via
  // arrow keys).
  mixer[kColorOmniboxResultsButtonInkDropRowSelected] = {ui::SetAlpha(
      kColorOmniboxResultsButtonInkDropSelected, std::ceil(0.10f * 255.0f))};
  mixer[kColorOmniboxResultsButtonInkDropSelectedRowSelected] = {ui::SetAlpha(
      kColorOmniboxResultsButtonInkDropSelected, std::ceil(0.16f * 255.0f))};
}

// Apply updates to the Omnibox color tokens per CR2023 guidelines.
void ApplyOmniboxCR2023Colors(ui::ColorMixer& mixer,
                              const ui::ColorProviderKey& key) {
  ApplyOmniboxCR2023FallbackColors(mixer, key);

  // Do not apply the full set of CR2023 Omnibox colors to clients using
  // high-contrast mode or a custom theme.
  // TODO(khalidpeer): Roll out full set of CR2023 color updates for
  //   high-contrast clients.
  // TODO(khalidpeer): Roll out full set of CR2023 color updates for themed
  //   clients.
  if (ShouldApplyHighContrastColors(key) || key.custom_theme) {
    return;
  }
  ApplyGM3OmniboxTextColor(mixer, key);
  ApplyCR2023OmniboxExpandedStateColors(mixer, key);
  ApplyCR2023OmniboxIconColors(mixer, key);
}

}  // namespace

void AddOmniboxColorMixer(ui::ColorProvider* provider,
                          const ui::ColorProviderKey& key) {
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

  // Omnibox highlight colors.
  mixer[kColorOmniboxSelectionBackground] = {
      ui::kColorTextfieldSelectionBackground};
  mixer[kColorOmniboxSelectionForeground] = {
      ui::kColorTextfieldSelectionForeground};

  // Bubble outline colors.
  mixer[kColorOmniboxBubbleOutline] = ui::SelectBasedOnDarkInput(
      kColorToolbarBackgroundSubtleEmphasis, gfx::kGoogleGrey100,
      SkColorSetA(gfx::kGoogleGrey900, 0x24));
  mixer[kColorOmniboxBubbleOutlineExperimentalKeywordMode] = {
      kColorOmniboxKeywordSelected};

  // Results background, chip, button, and focus colors.
  mixer[kColorOmniboxResultsBackground] =
      ui::GetColorWithMaxContrast(kColorOmniboxText);
  mixer[kColorOmniboxResultsBackgroundIPH] = {ui::kColorSysSurface2};
  mixer[kColorOmniboxResultsBackgroundHovered] = ui::BlendTowardMaxContrast(
      kColorOmniboxResultsBackground, gfx::kGoogleGreyAlpha200);
  mixer[kColorOmniboxResultsBackgroundSelected] = ui::BlendTowardMaxContrast(
      ui::GetColorWithMaxContrast(kColorOmniboxResultsTextSelected),
      gfx::kGoogleGreyAlpha200);
  mixer[kColorOmniboxResultsChipBackground] = {ui::kColorSysNeutralContainer};
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
                                     background_id, std::nullopt,
                                     contrast_ratio);
    };
    mixer[kColorOmniboxResultsIcon] =
        results_icon(kColorOmniboxText, kColorOmniboxResultsBackground);
    mixer[kColorOmniboxResultsIconSelected] =
        results_icon(kColorOmniboxResultsTextSelected,
                     kColorOmniboxResultsBackgroundSelected);
    mixer[kColorOmniboxResultsStarterPackIcon] = ui::BlendForMinContrast(
        gfx::kGoogleBlue600, kColorOmniboxResultsBackground, std::nullopt,
        color_utils::kMinimumVisibleContrastRatio);
  }

  // Dimmed text colors.
  {
    const auto blend_with_clamped_contrast =
        [contrast_ratio](ui::ColorId foreground_id, ui::ColorId background_id) {
          return ui::BlendForMinContrast(
              foreground_id, foreground_id,
              ui::BlendForMinContrast(background_id, background_id,
                                      std::nullopt, contrast_ratio),
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
          background, std::nullopt, contrast_ratio);
    };
    const auto positive_color = [contrast_ratio](
                                    ui::ColorId background,
                                    ui::ColorTransform dark_selector) {
      return ui::BlendForMinContrast(
          // Like kColorAlertLowSeverity, but toggled on `dark_selector`.
          ui::SelectBasedOnDarkInput(dark_selector, gfx::kGoogleGreen300,
                                     gfx::kGoogleGreen700),
          background, std::nullopt, contrast_ratio);
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
          background, std::nullopt, contrast_ratio);
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
          kColorToolbarBackgroundSubtleEmphasisHovered, std::nullopt,
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
    mixer[kColorOmniboxSecurityChipDangerousBackground] =
        ui::SelectBasedOnDarkInput(kColorOmniboxResultsBackground,
                                   gfx::kGoogleRed300, gfx::kGoogleRed800);
    mixer[kColorOmniboxSecurityChipText] = ui::SelectBasedOnDarkInput(
        kColorOmniboxSecurityChipDangerousBackground,
        ui::GetColorWithMaxContrast(
            kColorOmniboxSecurityChipDangerousBackground),
        gfx::kGoogleRed800);
    mixer[kColorOmniboxSecurityChipInkDropHover] = {
        ui::SetAlpha(kColorOmniboxSecurityChipText, std::ceil(0.10f * 255.0f))};
    mixer[kColorOmniboxSecurityChipInkDropRipple] = {
        ui::SetAlpha(kColorOmniboxSecurityChipText, std::ceil(0.16f * 255.0f))};
  }

  // TODO(manukh): `kColorOmniboxResultsIconGM3Background` is unused currently,
  //   but if we decide to revisit it, we should use tokens instead of rgb's.
  mixer[kColorOmniboxResultsIconGM3Background] = ui::SelectBasedOnDarkInput(
      kColorToolbar, SkColorSetRGB(48, 48, 48), SkColorSetRGB(242, 242, 242));
  mixer[kColorOmniboxAnswerIconGM3Background] = {ui::kColorSysTonalContainer};
  mixer[kColorOmniboxAnswerIconGM3Foreground] = {ui::kColorSysOnTonalContainer};

  // Location bar icon colors for opaque page info elements. There is no
  // distinction between regular and tonal page info backgrounds or foregrounds
  // for CWS themes.
  mixer[kColorPageInfoBackground] = {kColorToolbar};
  mixer[kColorPageInfoBackgroundTonal] = {kColorPageInfoBackground};
  mixer[kColorPageInfoForeground] = {ui::kColorSysOnSurface};
  mixer[kColorPageInfoSubtitleForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorPageInfoForegroundTonal] = {kColorPageInfoForeground};
  // Literal constants are `kOmniboxOpacityHovered` and
  // `kOmniboxOpacitySelected`. This is so that we can more cleanly use the
  // colors in the inkdrop instead of handling themes and non-themes separately
  // in-code as they have different opacity requirements.
  mixer[kColorPageInfoIconHover] = {
      ui::SetAlpha(kColorOmniboxText, std::ceil(0.10f * 255.0f))};
  mixer[kColorPageInfoIconPressed] = {
      ui::SetAlpha(kColorOmniboxText, std::ceil(0.16f * 255.0f))};
  mixer[kColorPageActionIconHover] = {kColorPageInfoIconHover};
  mixer[kColorPageActionIcon] = {kColorOmniboxResultsIcon};

  // Override omnibox colors per CR2023 spec.
  ApplyOmniboxCR2023Colors(mixer, key);
}
