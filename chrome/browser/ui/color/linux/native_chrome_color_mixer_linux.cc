// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/color/new_tab_page_color_mixer.h"
#include "components/search/ntp_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

ui::ColorTransform UseIfNonzeroAlpha(ui::ColorTransform transform) {
  const auto generator = [](ui::ColorTransform transform, SkColor input_color,
                            const ui::ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color =
        SkColorGetA(transform_color) ? transform_color : input_color;
    DVLOG(2) << "ColorTransform UseIfNonzeroAlpha:" << " Input Color: "
             << ui::SkColorName(input_color)
             << " Transform Color: " << ui::SkColorName(transform_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform));
}

ui::ColorTransform GetToolbarTopSeparatorColorTransform(
    ui::ColorTransform transform,
    bool high_contrast) {
  const auto generator = [](ui::ColorTransform transform, SkColor input_color,
                            const ui::ColorMixer& mixer) {
    const float kMinContrastRatio = 2.f;
    const SkColor toolbar = transform.Run(input_color, mixer);
    // Try a darker separator color first (even on dark themes).
    const SkColor separator =
        color_utils::BlendForMinContrast(toolbar, toolbar, gfx::kGoogleGrey900,
                                         kMinContrastRatio)
            .color;
    if (color_utils::GetContrastRatio(separator, toolbar) >=
        kMinContrastRatio) {
      return separator;
    }
    // If a darker separator didn't give good enough contrast, try a lighter
    // separator.
    return color_utils::BlendForMinContrast(toolbar, toolbar, SK_ColorWHITE,
                                            kMinContrastRatio)
        .color;
  };
  return high_contrast ? ui::GetColorWithMaxContrast(transform)
                       : base::BindRepeating(generator, std::move(transform));
}

// If the GTK/Qt menu colors and Chrome's light/dark setting agree (both dark
// or both light), keep Chrome's designated colors. If they are different,
// use a color based on the GTK/Qt theme instead.
//
// When Chrome uses the GTK or Qt themes, the menu's background and text
// colors come from that theme. However, other colors (cards, buttons, chips)
// come from Chrome's light/dark settings. These colors usually match, but it
// is not always the case.
//
// For example, a user can pick a dark GTK theme while having no light/dark
// preference set in their settings. Chrome will treat this as light
// (crrev.com/c/8429688). The menu would then show light cards with light text.
ui::ColorTransform UseNativeIfPaletteMismatch(ui::ColorTransform native,
                                              bool dark_mode) {
  const auto generator = [](ui::ColorTransform native, bool dark_mode,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const bool toolkit_is_dark =
        color_utils::IsDark(mixer.GetResultColor(ui::kColorMenuBackground));
    const SkColor result_color = toolkit_is_dark == dark_mode
                                     ? input_color
                                     : native.Run(input_color, mixer);
    DVLOG(2) << "ColorTransform UseNativeIfPaletteMismatch:"
             << " Input Color: " << ui::SkColorName(input_color)
             << " Toolkit Is Dark: " << toolkit_is_dark
             << " Dark Mode: " << dark_mode
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(native), dark_mode);
}

}  // namespace

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  if (key.system_theme == ui::SystemTheme::kDefault) {
    return;
  }

  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparatorDefault};
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIconDefault};
  mixer[kColorBookmarkFavicon] = {kColorToolbarButtonIcon};
  mixer[kColorInfoBarForeground] = {ui::kColorNativeLabelForeground};
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] =
      UseIfNonzeroAlpha(ui::kColorNativeTextfieldBorderUnfocused);
  mixer[kColorNewTabButtonBackgroundFrameActive] = {SK_ColorTRANSPARENT};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {SK_ColorTRANSPARENT};
  mixer[kColorNewTabPageBackground] = {ui::kColorTextfieldBackground};
  mixer[kColorNewTabPageHeader] = {ui::kColorNativeBoxFrameBorder};
  mixer[kColorNewTabPageLink] = {ui::kColorTextfieldSelectionBackground};
  mixer[kColorNewTabPageText] = {ui::kColorTextfieldForeground};
  mixer[kColorOmniboxText] = {ui::kColorTextfieldForeground};
  mixer[kColorToolbarBackgroundSubtleEmphasis] = {
      ui::kColorTextfieldBackground};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      ui::kColorNativeTabForegroundInactiveFrameActive};
  mixer[kColorTabForegroundInactiveFrameInactive] = {
      ui::kColorNativeTabForegroundInactiveFrameInactive};
  mixer[kColorTabBackgroundInactiveHoverFrameActive] = {
      ui::kColorSysStateHeaderHover};
  mixer[kColorTabBackgroundInactiveHoverFrameInactive] = {
      ui::kColorSysStateHeaderHoverInactive};
  mixer[kColorTabBackgroundSelectedFrameActive] = {ui::GetResultingPaintColor(
      ui::SetAlpha(ui::kColorAccent, 0x80),
      kColorTabBackgroundInactiveFrameActive)};
  mixer[kColorTabBackgroundSelectedFrameInactive] = {ui::GetResultingPaintColor(
      ui::SetAlpha(ui::kColorAccent, 0x80),
      kColorTabBackgroundInactiveFrameInactive)};
  mixer[kColorTabBackgroundSelectedHoverFrameActive] = {
      ui::GetResultingPaintColor(ui::kColorSysStateHoverDimBlendProtection,
                                 kColorTabBackgroundSelectedFrameActive)};
  mixer[kColorTabBackgroundSelectedHoverFrameInactive] = {
      ui::GetResultingPaintColor(ui::kColorSysStateHoverDimBlendProtection,
                                 kColorTabBackgroundSelectedFrameInactive)};
  mixer[kColorTabStrokeFrameActive] = {kColorToolbarTopSeparatorFrameActive};
  mixer[kColorTabStrokeFrameInactive] = {
      kColorToolbarTopSeparatorFrameInactive};
  mixer[kColorToolbar] = {ui::kColorNativeToolbarBackground};
  mixer[kColorToolbarButtonIcon] = {kColorToolbarText};
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarSeparator};
  mixer[kColorToolbarSeparator] = {ui::kColorNativeBoxFrameBorder};
  mixer[kColorToolbarText] = {ui::kColorNativeLabelForeground};
  mixer[kColorToolbarTextDisabled] =
      ui::SetAlpha(kColorToolbarText, gfx::kDisabledControlAlpha);
  const bool high_contrast =
      key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh;
  mixer[kColorToolbarTopSeparatorFrameActive] =
      GetToolbarTopSeparatorColorTransform(ui::kColorNativeToolbarBackground,
                                           high_contrast);
  mixer[kColorToolbarTopSeparatorFrameInactive] = {
      kColorToolbarTopSeparatorFrameActive};

  // App menu surfaces. Keep the upstream (Material) colors when the toolkit
  // palette matches color_mode; otherwise derive them from the toolkit's
  // menu colors. This way they stay legible with toolkit-provided menu text.
  const bool dark_mode =
      key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  const auto native_if_mismatch = [dark_mode](ui::ColorTransform native) {
    return UseNativeIfPaletteMismatch(std::move(native), dark_mode);
  };
  const ui::ColorTransform card_background = ui::AlphaBlend(
      ui::kColorMenuItemForeground, ui::kColorMenuBackground, /*alpha=*/0x14);
  const ui::ColorTransform subtle_hover =
      ui::SetAlpha(ui::kColorMenuItemForeground, /*alpha=*/0x1F);

  // Cards, rows and pills that sit on the menu background.
  mixer[kColorAppMenuYourChromeBackground] =
      native_if_mismatch(card_background);
  mixer[kColorAppMenuToolsAndActionsBackground] =
      native_if_mismatch(card_background);
  mixer[kColorAppMenuZoomButtonBackground] =
      native_if_mismatch(card_background);
  mixer[kColorAppMenuZoomButtonHover] = native_if_mismatch(subtle_hover);
  mixer[kColorAppMenuZoomSeparator] =
      native_if_mismatch(ui::kColorMenuSeparator);
  mixer[ui::kColorAppMenuProfileRowBackground] =
      native_if_mismatch(card_background);
  mixer[ui::kColorMenuButtonBackground] = native_if_mismatch(card_background);
  mixer[ui::kColorAppMenuUpgradeRowBackground] =
      native_if_mismatch(card_background);
  mixer[ui::kColorAppMenuUpgradeRowSubstringForeground] =
      native_if_mismatch(ui::kColorMenuItemForeground);
  // Hover for items inside cards and rows, in both the GlowUp and classic app
  // menus. Matches the block and footer button hover.
  mixer[ui::kColorAppMenuRowBackgroundHovered] =
      native_if_mismatch(subtle_hover);

  // Block buttons (New tab / New window / Incognito).
  mixer[kColorAppMenuBlockButtonBackground] =
      native_if_mismatch(ui::kColorMenuBackground);
  mixer[kColorAppMenuBlockButtonBackgroundHovered] =
      native_if_mismatch(subtle_hover);
  mixer[kColorAppMenuBlockButtonBorder] =
      native_if_mismatch(ui::kColorMenuSeparator);
  mixer[kColorAppMenuBlockButtonForeground] =
      native_if_mismatch(ui::kColorMenuItemForeground);

  // Footer buttons (Settings / About / Exit).
  mixer[kColorAppMenuFooterButtonForeground] =
      native_if_mismatch(ui::kColorMenuItemForeground);
  mixer[kColorAppMenuFooterButtonForegroundHovered] =
      native_if_mismatch(ui::kColorMenuItemForeground);
  mixer[kColorAppMenuFooterButtonBackgroundHovered] =
      native_if_mismatch(subtle_hover);

  // Chips use the toolkit's prominent (accent) pair, which the toolkit mixers
  // already compute with a contrast guarantee. The hovered variants are
  // derived from these IDs upstream, so they follow automatically.
  mixer[kColorAppMenuChipBackground] =
      native_if_mismatch(ui::kColorButtonBackgroundProminent);
  mixer[kColorAppMenuChipForeground] =
      native_if_mismatch(ui::kColorButtonForegroundProminent);
  mixer[ui::kColorAppMenuProfileRowChipBackground] =
      native_if_mismatch(ui::kColorButtonBackgroundProminent);
}
