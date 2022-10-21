// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/no_destructor.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/win/shell.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/views_features.h"

namespace {

SkColor GetDefaultInactiveFrameColor() {
  return base::win::GetVersion() < base::win::Version::WIN10
             ? SkColorSetRGB(0xEB, 0xEB, 0xEB)
             : SK_ColorWHITE;
}

// This class encapsulates much of the same logic from ThemeHelperWin pertaining
// to the calculation of frame colors on Windows 8, 10 and up. Once the
// ColorProvider is permanently switched on, all the relevant code from
// ThemeHelperWin can be deleted.
class FrameColorHelper {
 public:
  FrameColorHelper();
  FrameColorHelper(const FrameColorHelper&) = delete;
  FrameColorHelper& operator=(const FrameColorHelper&) = delete;
  ~FrameColorHelper() = default;

  void AddNativeChromeColors(ui::ColorMixer& mixer,
                             const ui::ColorProviderManager::Key& key) const;
  void AddBorderAccentColors(ui::ColorMixer& mixer) const;

  static FrameColorHelper* Get();

 private:
  // Returns whether there is a custom image provided for the given id.
  bool HasCustomImage(int id, const ui::ColorProviderManager::Key& key) const;

  // Returns true if colors from DWM can be used, i.e. this is a native frame
  // on Windows 8+.
  bool DwmColorsAllowed(const ui::ColorProviderManager::Key& key) const;

  // Returns the Tint for the given |id|. If there is no tint, the identity tint
  // {-1, -1, -1} is returned and won't tint the color on which it is used.
  color_utils::HSL GetTint(int id,
                           const ui::ColorProviderManager::Key& key) const;

  // Callback executed when the accent color is updated. This re-reads the
  // accent color and updates |dwm_frame_color_| and
  // |dwm_inactive_frame_color_|.
  void OnAccentColorUpdated();

  // Re-reads the accent colors and updates member variables.
  void FetchAccentColors();

  base::CallbackListSubscription subscription_ =
      ui::AccentColorObserver::Get()->Subscribe(
          base::BindRepeating(&FrameColorHelper::OnAccentColorUpdated,
                              base::Unretained(this)));

  // The frame color when active. If empty the default colors should be used.
  absl::optional<SkColor> dwm_frame_color_;

  // The frame color when inactive. If empty the default colors should be used.
  absl::optional<SkColor> dwm_inactive_frame_color_;

  // The DWM accent border color, if available; white otherwise.
  SkColor dwm_accent_border_color_;
};

FrameColorHelper::FrameColorHelper() {
  FetchAccentColors();
}

void FrameColorHelper::AddNativeChromeColors(
    ui::ColorMixer& mixer,
    const ui::ColorProviderManager::Key& key) const {
  using TP = ThemeProperties;
  auto get_theme_color = [key](int id) -> absl::optional<SkColor> {
    SkColor theme_color;
    if (key.custom_theme && key.custom_theme->GetColor(id, &theme_color))
      return theme_color;
    return absl::nullopt;
  };
  if (DwmColorsAllowed(key)) {
    // When we're custom-drawing the titlebar we want to use either the colors
    // we calculated in OnDwmKeyUpdated() or the default colors. When we're not
    // custom-drawing the titlebar we want to match the color Windows actually
    // uses because some things (like the incognito icon) use this color to
    // decide whether they should draw in light or dark mode. Incognito colors
    // should be the same as non-incognito in all cases here.
    if (auto color = get_theme_color(TP::COLOR_FRAME_ACTIVE))
      mixer[ui::kColorFrameActive] = {color.value()};
    else if (dwm_frame_color_)
      mixer[ui::kColorFrameActive] = {dwm_frame_color_.value()};
    else if (!ShouldCustomDrawSystemTitlebar())
      mixer[ui::kColorFrameActive] = {SK_ColorWHITE};

    if (auto color = get_theme_color(TP::COLOR_FRAME_INACTIVE)) {
      mixer[ui::kColorFrameInactive] = {color.value()};
    } else if (dwm_inactive_frame_color_) {
      mixer[ui::kColorFrameInactive] = {dwm_inactive_frame_color_.value()};
    } else if (!ShouldCustomDrawSystemTitlebar()) {
      mixer[ui::kColorFrameInactive] = {GetDefaultInactiveFrameColor()};
    } else if (dwm_frame_color_) {
      mixer[ui::kColorFrameInactive] =
          ui::HSLShift({dwm_frame_color_.value()},
                       GetTint(ThemeProperties::TINT_FRAME_INACTIVE, key));
    }
  } else {
    if (auto color = get_theme_color(TP::COLOR_FRAME_ACTIVE))
      mixer[ui::kColorFrameActive] = {color.value()};
    if (auto color = get_theme_color(TP::COLOR_FRAME_INACTIVE))
      mixer[ui::kColorFrameInactive] = {color.value()};
  }
}

void FrameColorHelper::AddBorderAccentColors(ui::ColorMixer& mixer) const {
  // In Windows 10, native inactive borders are #555555 with 50% alpha.
  // Prior to version 1809, native active borders use the accent color.
  // In version 1809 and following, the active border is #262626 with 66%
  // alpha unless the accent color is also used for the frame.
  mixer[kColorAccentBorderActive] = {
      (base::win::GetVersion() >= base::win::Version::WIN10_RS5 &&
       !dwm_frame_color_)
          ? SkColorSetARGB(0xa8, 0x26, 0x26, 0x26)
          : dwm_accent_border_color_};
  mixer[kColorAccentBorderInactive] = {SkColorSetARGB(0x80, 0x55, 0x55, 0x55)};
}

// static
FrameColorHelper* FrameColorHelper::Get() {
  static base::NoDestructor<FrameColorHelper> g_frame_color_helper;
  return g_frame_color_helper.get();
}

bool FrameColorHelper::HasCustomImage(
    int id,
    const ui::ColorProviderManager::Key& key) const {
  return BrowserThemePack::IsPersistentImageID(id) && key.custom_theme &&
         key.custom_theme->HasCustomImage(id);
}

bool FrameColorHelper::DwmColorsAllowed(
    const ui::ColorProviderManager::Key& key) const {
  const bool use_native_frame_if_enabled =
      (ShouldCustomDrawSystemTitlebar() ||
       !HasCustomImage(IDR_THEME_FRAME, key)) &&
      (base::win::GetVersion() >= base::win::Version::WIN8);
  return use_native_frame_if_enabled && ui::win::IsAeroGlassEnabled();
}

color_utils::HSL FrameColorHelper::GetTint(
    int id,
    const ui::ColorProviderManager::Key& key) const {
  color_utils::HSL hsl;
  if (key.custom_theme && key.custom_theme->GetTint(id, &hsl))
    return hsl;
  // Always pass false for |incognito| here since the ColorProvider is treating
  // incognito mode as dark mode. If this needs to change, that information will
  // need to propagate into the ColorProviderManager::Key.
  return ThemeProperties::GetDefaultTint(
      id, false, key.color_mode == ui::ColorProviderManager::ColorMode::kDark);
}

void FrameColorHelper::OnAccentColorUpdated() {
  FetchAccentColors();
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

void FrameColorHelper::FetchAccentColors() {
  const auto* accent_color_observer = ui::AccentColorObserver::Get();
  dwm_accent_border_color_ =
      accent_color_observer->accent_border_color().value_or(
          GetDefaultInactiveFrameColor());

  if (base::win::GetVersion() < base::win::Version::WIN10) {
    dwm_frame_color_ = dwm_accent_border_color_;
  } else {
    dwm_frame_color_ = accent_color_observer->accent_color();
    dwm_inactive_frame_color_ = accent_color_observer->accent_color_inactive();
  }
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

}  // namespace

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // NOTE: These cases are always handled, even on Win7, in order to ensure the
  // the color provider redirection tests function. Win7 callers should never
  // actually pass in these IDs.
  FrameColorHelper::Get()->AddBorderAccentColors(mixer);

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
  mixer[kColorTryChromeBackground] = {SkColorSetRGB(0x1F, 0x1F, 0x1F)};
  mixer[kColorTryChromeBorder] = {SkColorSetARGB(0x80, 0x80, 0x80, 0x80)};
  mixer[kColorTryChromeButtonBackgroundAccept] = {
      SkColorSetRGB(0x00, 0x78, 0xDA)};
  mixer[kColorTryChromeButtonBackgroundNoThanks] = {
      SkColorSetA(SK_ColorWHITE, 0x33)};
  mixer[kColorTryChromeButtonForeground] = {SK_ColorWHITE};
  mixer[kColorTryChromeForeground] = {SkColorSetA(SK_ColorWHITE, 0xAD)};
  mixer[kColorTryChromeHeaderForeground] = {SK_ColorWHITE};

  if (key.color_mode == ui::ColorProviderManager::ColorMode::kLight) {
    mixer[kColorNewTabPageBackground] = {ui::kColorNativeWindow};
    mixer[kColorNewTabPageLink] = {ui::kColorNativeHotlight};
    mixer[kColorNewTabPageText] = {ui::kColorNativeWindowText};
  }

  if (key.contrast_mode != ui::ColorProviderManager::ContrastMode::kHigh) {
    FrameColorHelper::Get()->AddNativeChromeColors(mixer, key);
    return;
  }

  // High contrast uses system colors.
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
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
  mixer[kColorOmniboxResultsBackgroundHovered] = {
      kColorOmniboxResultsBackgroundSelected};
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
  mixer[kColorToolbar] = {ui::kColorNativeWindow};
  mixer[kColorToolbarButtonIcon] = {kColorToolbarText};
  const bool platform_high_contrast_ink_drop = base::FeatureList::IsEnabled(
      views::features::kEnablePlatformHighContrastInkDrop);
  if (platform_high_contrast_ink_drop)
    mixer[kColorToolbarButtonIconHovered] = {ui::kColorNativeHighlightText};
  else
    mixer[kColorToolbarButtonIconHovered] = {kColorToolbarText};
  mixer[kColorToolbarButtonIconInactive] = {ui::kColorNativeGrayText};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarText};
  if (platform_high_contrast_ink_drop)
    mixer[kColorToolbarInkDrop] = {ui::kColorNativeHighlight};
  mixer[kColorToolbarSeparator] = {ui::kColorNativeWindowText};
  mixer[kColorToolbarText] = {ui::kColorNativeBtnText};
  mixer[kColorToolbarTopSeparatorFrameActive] = {kColorToolbarSeparator};
  mixer[kColorToolbarTopSeparatorFrameInactive] = {
      kColorToolbarTopSeparatorFrameActive};
}
