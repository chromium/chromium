// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/win/mica_titlebar.h"
#include "chrome/grit/theme_resources.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/gfx/color_utils.h"

namespace {

color_utils::HSL GetTint(int id, const ui::ColorProviderKey& key) {
  const color_utils::HSL default_tint = ThemeProperties::GetDefaultTint(
      id,
      // The `ColorProviderKey` does not provide separate incognito state, but
      // the color mode will be dark in that case.
      // TODO(pkasting): `ThemeService`/`ThemeProperties` plumbing incognito and
      // dark mode separately is largely a broken relic at this point, and
      // various callers already conflate the two in both directions. The
      // "incognito" concept should probably be purged from all that code, and
      // for the very few places that make a distinction (e.g. frame tints) we
      // should just pick which behavior we want.
      false, key.color_mode == ui::ColorProviderKey::ColorMode::kDark);

  color_utils::HSL custom_tint;
  return (key.custom_theme && key.custom_theme->GetTint(id, &custom_tint))
             ? custom_tint
             : default_tint;
}

std::optional<SkColor> GetThemeColor(const ui::ColorProviderKey& key, int id) {
  SkColor theme_color;
  return (key.custom_theme && key.custom_theme->GetColor(id, &theme_color))
             ? std::make_optional(theme_color)
             : std::nullopt;
}

struct FrameTransforms {
  std::optional<ui::ColorTransform> active;
  std::optional<ui::ColorTransform> inactive;
};

FrameTransforms GetMicaFrameTransforms(const ui::ColorProviderKey& key) {
  const auto mica_frame_color =
      (key.color_mode == ui::ColorProviderKey::ColorMode::kDark)
          ? SkColorSetRGB(0x20, 0x20, 0x20)
          : SkColorSetRGB(0xE8, 0xE8, 0xE8);
  return {mica_frame_color, mica_frame_color};
}

FrameTransforms GetSystemFrameTransforms(const ui::ColorProviderKey& key) {
  FrameTransforms frame_transforms;
  if (ShouldDefaultThemeUseMicaTitlebar()) {
    frame_transforms = GetMicaFrameTransforms(key);
  }
  if (const auto* const accent_color_observer = ui::AccentColorObserver::Get();
      accent_color_observer->ShouldUseAccentColorForWindowFrame()) {
    if (const std::optional<SkColor> dwm_frame_color =
            accent_color_observer->accent_color()) {
      frame_transforms.active = {dwm_frame_color.value()};
      const std::optional<SkColor> dwm_inactive_frame_color =
          accent_color_observer->accent_color_inactive();
      frame_transforms.inactive =
          dwm_inactive_frame_color.has_value()
              ? ui::ColorTransform(dwm_inactive_frame_color.value())
              : ui::HSLShift(
                    {dwm_frame_color.value()},
                    GetTint(ThemeProperties::TINT_FRAME_INACTIVE, key));
    }
  }
  return frame_transforms;
}

FrameTransforms GetFrameTransforms(const ui::ColorProviderKey& key) {
  FrameTransforms frame_transforms;
  if (key.frame_style == ui::ColorProviderKey::FrameStyle::kSystem) {
    frame_transforms = GetSystemFrameTransforms(key);
  }
  if (auto color = GetThemeColor(key, ThemeProperties::COLOR_FRAME_ACTIVE)) {
    frame_transforms.active = {color.value()};
  }
  if (auto color = GetThemeColor(key, ThemeProperties::COLOR_FRAME_INACTIVE)) {
    frame_transforms.inactive = {color.value()};
  }
  return frame_transforms;
}

void EnsureColorProviderCacheWillBeResetWhenAccentColorStateChanges() {
  static base::NoDestructor<base::CallbackListSubscription> subscription(
      ui::AccentColorObserver::Get()->Subscribe(base::BindRepeating(
          // CAUTION: Do not bind directly to `ui::ColorProviderManager::Get()`
          // here, as tests may reset that value!
          [] { ui::ColorProviderManager::Get().ResetColorProviderCache(); })));
}

SkColor GetAccentBorderColor() {
  if (const auto* const accent_color_observer = ui::AccentColorObserver::Get();
      accent_color_observer->ShouldUseAccentColorForWindowFrame()) {
    if (const std::optional<SkColor> accent_border_color =
            accent_color_observer->accent_border_color()) {
      return accent_border_color.value();
    }
  }

  // Windows 10 pre-version 1809 native active borders default to white, while
  // in version 1809 and onwards they default to #262626 with 66% alpha.
  const bool pre_1809 = base::win::GetVersion() < base::win::Version::WIN10_RS5;
  return pre_1809 ? SK_ColorWHITE : SkColorSetARGB(0xa8, 0x26, 0x26, 0x26);
}

ui::ColorTransform GetCaptionForegroundColor(
    ui::ColorTransform input_transform) {
  const auto generator = [](ui::ColorTransform input_transform,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor background_color = input_transform.Run(input_color, mixer);
    const float windows_luma = 0.25f * SkColorGetR(background_color) +
                               0.625f * SkColorGetG(background_color) +
                               0.125f * SkColorGetB(background_color);
    const SkColor result_color =
        (windows_luma <= 128.0f) ? SK_ColorWHITE : SK_ColorBLACK;
    DVLOG(2) << "ColorTransform GetCaptionForegroundColor:"
             << " Background Color: " << ui::SkColorName(background_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(input_transform));
}

void AddNativeHighContrastColors(ui::ColorMixer& mixer) {
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] = {ui::kColorNativeWindowText};
  mixer[kColorToolbarBackgroundSubtleEmphasis] = {ui::kColorNativeBtnFace};
  mixer[kColorToolbarBackgroundSubtleEmphasisHovered] = {
      kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorOmniboxBubbleOutline] = {kColorOmniboxText};
  mixer[kColorOmniboxKeywordSelected] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsBackground] = {
      kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorOmniboxResultsBackgroundHovered] = {ui::kColorNativeHighlight};
  mixer[kColorOmniboxResultsBackgroundSelected] = {ui::kColorNativeHighlight};
  mixer[kColorOmniboxResultsIcon] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsIconSelected] = {kColorOmniboxResultsTextSelected};
  mixer[kColorOmniboxResultsTextDimmed] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsTextDimmedSelected] = {
      kColorOmniboxResultsTextSelected};
  mixer[kColorOmniboxResultsTextNegative] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsTextNegativeSelected] = {
      kColorOmniboxResultsTextSelected};
  mixer[kColorOmniboxResultsTextPositive] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsTextPositiveSelected] = {
      kColorOmniboxResultsTextSelected};
  mixer[kColorOmniboxResultsTextSecondary] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsTextSecondarySelected] = {
      kColorOmniboxResultsTextSelected};
  mixer[kColorOmniboxResultsTextSelected] = {ui::kColorNativeHighlightText};
  mixer[kColorOmniboxResultsUrl] = {kColorOmniboxText};
  mixer[kColorOmniboxResultsUrlSelected] = {kColorOmniboxResultsTextSelected};
  mixer[kColorOmniboxSecurityChipDangerous] = {kColorOmniboxText};
  mixer[kColorOmniboxSecurityChipDefault] = {kColorOmniboxText};
  mixer[kColorOmniboxSecurityChipSecure] = {kColorOmniboxText};
  mixer[kColorOmniboxText] = {ui::kColorTextfieldForeground};
  mixer[kColorOmniboxTextDimmed] = {kColorOmniboxText};
  mixer[kColorTabBackgroundActiveFrameActive] = {ui::kColorNativeHighlight};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabForegroundActiveFrameActive] = {ui::kColorNativeHighlightText};
  mixer[kColorNewTabButtonCRBackgroundFrameActive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorNewTabButtonCRBackgroundFrameInactive] = {
      kColorTabBackgroundActiveFrameInactive};
  mixer[kColorNewTabButtonForegroundFrameActive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorNewTabButtonForegroundFrameInactive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTaskManagerTableBackgroundSelectedFocused] = {
      ui::kColorNativeHighlight};
  mixer[kColorToolbar] = {ui::kColorNativeWindow};
  mixer[kColorToolbarButtonIcon] = {kColorToolbarText};
  mixer[kColorToolbarButtonIconHovered] = {ui::kColorNativeHighlightText};
  mixer[kColorToolbarButtonIconInactive] = {ui::kColorNativeGrayText};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarText};
  mixer[kColorToolbarInkDrop] = {ui::kColorNativeHighlight};
  mixer[kColorToolbarSeparator] = {ui::kColorNativeWindowText};
  mixer[kColorToolbarText] = {ui::kColorNativeBtnText};
  mixer[kColorToolbarTopSeparatorFrameActive] = {kColorToolbarSeparator};
  mixer[kColorToolbarTopSeparatorFrameInactive] = {
      kColorToolbarTopSeparatorFrameActive};
}

void AddNativeNonHighContrastColors(ui::ColorMixer& mixer,
                                    const ui::ColorProviderKey& key) {
  // Set frame colors appropriately.
  //
  // Instead of simply setting the frame colors directly, this sets the
  // underlying header colors. Although this diverges from Chrome's material
  // spec, these overrides are necessary to ensure UI assigned to these color
  // roles can continue to work as expected while respecting platform frame
  // overrides.
  const FrameTransforms frame_transforms = GetFrameTransforms(key);
  if (frame_transforms.active) {
    mixer[ui::kColorSysHeader] = frame_transforms.active.value();
    mixer[ui::kColorSysOnHeaderDivider] =
        GetColorWithMaxContrast(ui::kColorSysHeader);
    mixer[ui::kColorSysOnHeaderPrimary] =
        GetColorWithMaxContrast(ui::kColorSysHeader);
    mixer[ui::kColorSysStateHeaderHover] =
        ui::AlphaBlend(ui::kColorSysBase, ui::kColorSysHeader, 0x66);
    mixer[ui::kColorSysHeaderContainer] = {ui::kColorSysBase};
  }
  if (frame_transforms.inactive) {
    mixer[ui::kColorSysHeaderInactive] = frame_transforms.inactive.value();
    mixer[ui::kColorSysOnHeaderDividerInactive] =
        GetColorWithMaxContrast(ui::kColorSysHeaderInactive);
    mixer[ui::kColorSysOnHeaderPrimaryInactive] =
        GetColorWithMaxContrast(ui::kColorSysHeaderInactive);
    mixer[ui::kColorSysStateHeaderHoverInactive] =
        ui::AlphaBlend(ui::kColorSysBase, ui::kColorSysHeaderInactive, 0x66);
    mixer[ui::kColorSysHeaderContainerInactive] = {ui::kColorSysBase};
  }

  if (ShouldDefaultThemeUseMicaTitlebar() && !key.app_controller) {
    mixer[kColorNewTabButtonBackgroundFrameActive] = {SK_ColorTRANSPARENT};
    mixer[kColorNewTabButtonBackgroundFrameInactive] = {SK_ColorTRANSPARENT};
    mixer[kColorNewTabButtonInkDropFrameActive] =
        ui::GetColorWithMaxContrast(ui::kColorFrameActive);
    mixer[kColorNewTabButtonInkDropFrameInactive] =
        ui::GetColorWithMaxContrast(ui::kColorFrameInactive);
  }
}

}  // namespace

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  // If anything related to the accent color state changes, the color provider
  // cache should be reset, so that changes to the recipes below are picked up
  // even if the browser frame's color provider key does not change.
  //
  // When `ui::AccentColorObserver::accent_color()` itself changes, this happens
  // anyway, because the change causes `ui::OsSettingsProviderWin` to call
  // `ui::NativeTheme::NotifyOnNativeThemeUpdated()`, which will also reset the
  // cache. However, changes to other accent-color-related state (e.g.
  // `ui::AccentColorObserver::accent_border_color()`) will not (and should not)
  // trigger this codepath, but can still affect the recipes below and thus
  // require a reset.
  EnsureColorProviderCacheWillBeResetWhenAccentColorStateChanges();

  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorAccentBorderActive] = {GetAccentBorderColor()};
  mixer[kColorAccentBorderInactive] = {SkColorSetARGB(0x80, 0x55, 0x55, 0x55)};
  mixer[kColorCaptionButtonForegroundActive] =
      GetCaptionForegroundColor(kColorWindowControlButtonBackgroundActive);
  mixer[kColorCaptionButtonForegroundInactive] =
      GetCaptionForegroundColor(kColorWindowControlButtonBackgroundInactive);
  mixer[kColorCaptionCloseButtonBackgroundHovered] = {
      SkColorSetRGB(0xE8, 0x11, 0x23)};
  mixer[kColorCaptionCloseButtonForegroundHovered] = {SK_ColorWHITE};
  mixer[kColorCaptionForegroundActive] =
      GetCaptionForegroundColor(ui::kColorFrameActive);
  mixer[kColorCaptionForegroundInactive] =
      SetAlpha(GetCaptionForegroundColor(ui::kColorFrameInactive), 0x66);
  mixer[kColorTabSearchCaptionButtonFocusRing] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, ui::kColorFrameActive,
      color_utils::kMinimumVisibleContrastRatio);

  if (key.color_mode == ui::ColorProviderKey::ColorMode::kLight) {
    mixer[kColorNewTabPageBackground] = {ui::kColorNativeWindow};
    mixer[kColorNewTabPageLink] = {ui::kColorNativeHotlight};
    mixer[kColorNewTabPageText] = {ui::kColorNativeWindowText};
  }

  if (key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh) {
    AddNativeHighContrastColors(mixer);
  } else {
    AddNativeNonHighContrastColors(mixer, key);
  }
}
