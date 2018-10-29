// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_theme.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "ui/native_theme/native_theme_dark_aura.h"
#endif

#if defined(USE_X11)
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace {

SkColor GetColorFromNativeTheme(ui::NativeTheme::ColorId color_id,
                                OmniboxTint tint) {
  ui::NativeTheme* native_theme = nullptr;
#if defined(USE_AURA) || defined(OS_MACOSX)
  if (tint == OmniboxTint::DARK)
    native_theme = ui::NativeThemeDarkAura::instance();
#endif
#if defined(USE_X11)
  // Note: passing null to GetNativeTheme() always returns the native GTK theme.
  if (tint == OmniboxTint::NATIVE && views::LinuxUI::instance())
    native_theme = views::LinuxUI::instance()->GetNativeTheme(nullptr);
#endif
  if (!native_theme)
    native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  return native_theme->GetSystemColor(color_id);
}

SkColor GetSecurityChipColor(OmniboxTint tint, OmniboxPartState state) {
  if (tint == OmniboxTint::DARK)
    return gfx::kGoogleGrey200;
  return (state == OmniboxPartState::CHIP_DANGEROUS) ? gfx::kGoogleRed600
                                                     : gfx::kChromeIconGrey;
}

}  // namespace

SkColor GetOmniboxColor(OmniboxPart part,
                        OmniboxTint tint,
                        OmniboxPartState state) {
  using NativeId = ui::NativeTheme::ColorId;

  // Note this will use LIGHT for OmniboxTint::NATIVE.
  // TODO(https://crbug.com/819452): Determine the role GTK should play in this.
  bool dark = tint == OmniboxTint::DARK;

  // For high contrast, selected rows use inverted colors to stand out more.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  bool high_contrast = native_theme && native_theme->UsesHighContrastColors();
  bool selected = state == OmniboxPartState::SELECTED ||
                  state == OmniboxPartState::HOVERED_AND_SELECTED;
  if (high_contrast && selected)
    dark = !dark;

  switch (part) {
    case OmniboxPart::LOCATION_BAR_BACKGROUND: {
      const bool hovered = state == OmniboxPartState::HOVERED;
      return dark ? (hovered ? SkColorSetRGB(0x2F, 0x33, 0x36)
                             : SkColorSetRGB(0x28, 0x2C, 0x2F))
                  : (hovered ? gfx::kGoogleGrey200 : gfx::kGoogleGrey100);
    }
    case OmniboxPart::LOCATION_BAR_SECURITY_CHIP:
      return GetSecurityChipColor(tint, state);
    case OmniboxPart::LOCATION_BAR_SELECTED_KEYWORD:
      return dark ? gfx::kGoogleGrey100 : gfx::kGoogleBlue600;
    case OmniboxPart::LOCATION_BAR_FOCUS_RING:
      return dark ? gfx::kGoogleBlueDark600 : gfx::kGoogleBlue600;
    case OmniboxPart::RESULTS_BACKGROUND:
      // The spec calls for transparent black (or white) overlays for hover (8%)
      // and select (6%), which can overlap (for 14%). Pre-blend these with the
      // background for the best text AA result.
      // High contrast mode needs a darker base - Grey 800 with 14% white
      // overlaid on it (see below) is hard to produce good contrast ratios
      // against with colors other than white.
      return color_utils::BlendTowardOppositeLuma(
          dark ? (high_contrast ? gfx::kGoogleGrey900 : gfx::kGoogleGrey800)
               : SK_ColorWHITE,
          gfx::ToRoundedInt(GetOmniboxStateAlpha(state) * 0xff));
    case OmniboxPart::LOCATION_BAR_CLEAR_ALL:
    case OmniboxPart::LOCATION_BAR_TEXT_DEFAULT:
    case OmniboxPart::RESULTS_TEXT_DEFAULT:
      return dark ? gfx::kGoogleGrey100 : gfx::kGoogleGrey900;

    case OmniboxPart::LOCATION_BAR_TEXT_DIMMED:
      return dark ? gfx::kGoogleGrey500 : gfx::kGoogleGrey600;
    case OmniboxPart::RESULTS_ICON:
    case OmniboxPart::RESULTS_TEXT_DIMMED:
      // This is a pre-lightened (or darkened) variant of the base text color.
      return dark ? gfx::kGoogleGrey400 : gfx::kGoogleGrey700;

    case OmniboxPart::RESULTS_TEXT_INVISIBLE:
      return SK_ColorTRANSPARENT;
    case OmniboxPart::RESULTS_TEXT_NEGATIVE:
      return dark ? gfx::kGoogleRedDark600 : gfx::kGoogleRed600;
    case OmniboxPart::RESULTS_TEXT_POSITIVE:
      return dark ? gfx::kGoogleGreenDark600 : gfx::kGoogleGreen600;
    case OmniboxPart::RESULTS_TEXT_URL:
      if (high_contrast)
        return dark ? gfx::kGoogleBlue300 : gfx::kGoogleBlue700;
      return dark ? gfx::kGoogleBlueDark600 : gfx::kGoogleBlue600;

    case OmniboxPart::LOCATION_BAR_BUBBLE_OUTLINE:
      if (OmniboxFieldTrial::IsExperimentalKeywordModeEnabled())
        return gfx::kGoogleBlue700;
      return dark ? gfx::kGoogleGrey100
                  : SkColorSetA(gfx::kGoogleGrey900, 0x24);

    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_BACKGROUND:
      return GetColorFromNativeTheme(
          NativeId::kColorId_TextfieldSelectionBackgroundFocused, tint);

    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_TEXT:
      return GetColorFromNativeTheme(NativeId::kColorId_TextfieldSelectionColor,
                                     tint);
  }
  return gfx::kPlaceholderColor;
}

float GetOmniboxStateAlpha(OmniboxPartState state) {
  switch (state) {
    case OmniboxPartState::NORMAL:
      return 0;
    case OmniboxPartState::HOVERED:
      return 0.08f;
    case OmniboxPartState::SELECTED:
      return 0.06f;
    case OmniboxPartState::HOVERED_AND_SELECTED:
      return GetOmniboxStateAlpha(OmniboxPartState::HOVERED) +
             GetOmniboxStateAlpha(OmniboxPartState::SELECTED);
    default:
      NOTREACHED();
      return 0;
  }
}
