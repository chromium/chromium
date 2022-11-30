// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/color_scheme.h"

#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "chrome/browser/vr/assets_loader.h"
#include "ui/gfx/color_palette.h"

namespace vr {

namespace {

SkColor BuildColor(uint32_t color, int percentage) {
  DCHECK_GE(percentage, 0);
  DCHECK_LE(percentage, 100);
  return (static_cast<uint8_t>((2.55f * percentage + 0.5)) << 24) | color;
}

SkColor MakeColor(uint32_t color, int percentage) {
  DCHECK(!(color & 0xFF000000));
  return BuildColor(color, percentage);
}

SkColor MakeBlack(int percentage) {
  return BuildColor(0, percentage);
}

SkColor MakeWhite(int percentage) {
  return BuildColor(0xFFFFFF, percentage);
}

base::LazyInstance<ColorScheme>::Leaky g_normal_scheme =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ColorScheme>::Leaky g_fullscreen_scheme =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ColorScheme>::Leaky g_incognito_scheme =
    LAZY_INSTANCE_INITIALIZER;

void InitializeColorSchemes() {
  static bool initialized = false;
  if (initialized)
    return;

  ColorScheme& normal_scheme = g_normal_scheme.Get();
  normal_scheme.world_background = 0xFF999999;
  normal_scheme.floor = 0xFF8C8C8C;
  normal_scheme.ceiling = normal_scheme.floor;
  normal_scheme.floor_grid = 0x26FFFFFF;
  normal_scheme.web_vr_background = SK_ColorBLACK;
  normal_scheme.disc_button_colors.foreground = 0x87000000;
  normal_scheme.disc_button_colors.foreground_disabled = 0x33333333;
  normal_scheme.disc_button_colors.background = 0xCCB3B3B3;
  normal_scheme.disc_button_colors.background_hover = 0xCCE3E3E3;
  normal_scheme.disc_button_colors.background_down = 0xCCF3F3F3;
  normal_scheme.web_vr_floor_edge = SK_ColorBLACK;
  normal_scheme.web_vr_floor_center = 0xD9212121;
  normal_scheme.web_vr_floor_grid = 0xD9212121;
  normal_scheme.loading_indicator_foreground = MakeColor(0x4285F4, 100);
  normal_scheme.loading_indicator_background = MakeColor(0xD1E1FC, 100);
  normal_scheme.exit_warning_foreground = SK_ColorWHITE;
  normal_scheme.exit_warning_background = 0xCC1A1A1A;
  normal_scheme.web_vr_transient_toast_foreground = 0xFFF3F3F3;
  normal_scheme.web_vr_transient_toast_background = SK_ColorBLACK;
  normal_scheme.toast_foreground = 0xCCFFFFFF;
  normal_scheme.toast_background = 0xCC2F2F2F;
  normal_scheme.modal_prompt_icon_foreground = 0xFF4285F4;
  normal_scheme.modal_prompt_background = 0xFFF5F5F5;
  normal_scheme.modal_prompt_foreground = 0xFF333333;
  normal_scheme.modal_prompt_secondary_button_colors.foreground = 0xFF4285F4;
  normal_scheme.modal_prompt_secondary_button_colors.foreground_disabled =
      normal_scheme.disc_button_colors.foreground_disabled;
  normal_scheme.modal_prompt_secondary_button_colors.background =
      normal_scheme.modal_prompt_background;
  normal_scheme.modal_prompt_secondary_button_colors.background_hover =
      0x19999999;
  normal_scheme.modal_prompt_secondary_button_colors.background_down =
      0x33999999;
  normal_scheme.modal_prompt_primary_button_colors.foreground =
      normal_scheme.modal_prompt_background;
  normal_scheme.modal_prompt_secondary_button_colors.foreground_disabled =
      normal_scheme.disc_button_colors.foreground_disabled;
  normal_scheme.modal_prompt_primary_button_colors.background = 0xFF4285F4;
  normal_scheme.modal_prompt_primary_button_colors.background_hover =
      0xFF3E7DE6;
  normal_scheme.modal_prompt_primary_button_colors.background_down = 0xFF3E7DE6;
  normal_scheme.prompt_foreground = 0xCC000000;
  normal_scheme.prompt_primary_button_colors.foreground = 0xA6000000;
  normal_scheme.prompt_primary_button_colors.foreground_disabled = 0xA6000000;
  normal_scheme.prompt_primary_button_colors.background = 0xBFFFFFFF;
  normal_scheme.prompt_primary_button_colors.background_hover = 0xFFFFFFFF;
  normal_scheme.prompt_primary_button_colors.background_down = 0xE6FFFFFF;
  normal_scheme.prompt_secondary_button_colors.foreground = 0xA6000000;
  normal_scheme.prompt_secondary_button_colors.foreground_disabled = 0xA6000000;
  normal_scheme.prompt_secondary_button_colors.background = 0x66FFFFFF;
  normal_scheme.prompt_secondary_button_colors.background_hover = 0xFFFFFFFF;
  normal_scheme.prompt_secondary_button_colors.background_down = 0xE6FFFFFF;

  normal_scheme.url_bar_background = 0xCCB3B3B3;
  normal_scheme.url_bar_separator = MakeBlack(12);
  normal_scheme.url_bar_text = MakeBlack(65);
  normal_scheme.url_bar_hint_text = MakeBlack(50);
  normal_scheme.url_bar_dangerous_icon = gfx::kGoogleRed700;
  normal_scheme.url_bar_button.background = SK_ColorTRANSPARENT;
  normal_scheme.url_bar_button.background_hover = MakeBlack(8);
  normal_scheme.url_bar_button.background_down = MakeBlack(8);
  normal_scheme.url_bar_button.foreground = MakeBlack(65);
  normal_scheme.url_bar_button.foreground_disabled = MakeBlack(24);
  normal_scheme.url_text_emphasized = MakeBlack(80);
  normal_scheme.url_text_deemphasized = MakeBlack(30);
  normal_scheme.menu_text = MakeBlack(87);
  normal_scheme.omnibox_background = 0xFFEEEEEE;
  normal_scheme.omnibox_text_selection.cursor = 0xFF5595FE;      // TODO
  normal_scheme.omnibox_text_selection.background = 0xFFC6DAFC;  // TODO
  normal_scheme.omnibox_text_selection.foreground = normal_scheme.url_bar_text;
  normal_scheme.hyperlink = MakeColor(0x4285F4, 100);

  normal_scheme.dimmer_inner = 0xCC0D0D0D;
  normal_scheme.dimmer_outer = 0xE6000000;
  normal_scheme.splash_screen_background = SK_ColorBLACK;
  normal_scheme.splash_screen_text_color = 0xA6FFFFFF;
  normal_scheme.web_vr_timeout_spinner = 0xFFF3F3F3;
  normal_scheme.web_vr_timeout_message_background = 0xFF444444;
  normal_scheme.web_vr_timeout_message_foreground =
      normal_scheme.web_vr_timeout_spinner;
  normal_scheme.speech_recognition_circle_background = 0xFF4285F4;
  normal_scheme.snackbar_foreground = 0xFFEEEEEE;
  normal_scheme.snackbar_background = 0xDD212121;
  normal_scheme.snackbar_button_colors.background =
      normal_scheme.snackbar_background;
  normal_scheme.snackbar_button_colors.foreground = 0xFFFFD500;
  normal_scheme.snackbar_button_colors.background_hover = 0xDD2D2D2D;
  normal_scheme.snackbar_button_colors.background_down = 0xDD2D2D2D;

  normal_scheme.controller_label_callout = SK_ColorWHITE;
  normal_scheme.controller_button = 0xFFEFEFEF;
  normal_scheme.controller_button_down = 0xFF2979FF;
  normal_scheme.controller_battery_full = 0xFFEFEFEF;
  normal_scheme.controller_battery_empty = 0xCCB3B3B3;

  normal_scheme.reposition_label = SK_ColorWHITE;
  normal_scheme.reposition_label_background = 0xAA333333;

  normal_scheme.normal_factor = 1.0f;
  normal_scheme.incognito_factor = 0.0f;
  normal_scheme.fullscreen_factor = 0.0f;

  normal_scheme.content_reposition_frame = 0x66FFFFFF;

  normal_scheme.cursor_background_center = 0x66000000;
  normal_scheme.cursor_background_edge = SK_ColorTRANSPARENT;
  normal_scheme.cursor_foreground = SK_ColorWHITE;

  normal_scheme.webvr_permission_background = 0xD9212121;
  normal_scheme.webvr_permission_foreground = SK_ColorWHITE;

  normal_scheme.indicator.background = 0x73212121;
  normal_scheme.indicator.background_hover = 0xDE212121;
  normal_scheme.indicator.background_down = 0xDE212121;
  normal_scheme.indicator.foreground = SK_ColorWHITE;
  normal_scheme.indicator.foreground_disabled = SK_ColorWHITE;

  g_fullscreen_scheme.Get() = normal_scheme;
  ColorScheme& fullscreen_scheme = g_fullscreen_scheme.Get();
  fullscreen_scheme.world_background = 0xFF000714;
  fullscreen_scheme.floor = 0xFF070F1C;
  fullscreen_scheme.ceiling = 0xFF04080F;
  fullscreen_scheme.floor_grid = 0x40A3E0FF;

  fullscreen_scheme.disc_button_colors.foreground = 0x80FFFFFF;
  fullscreen_scheme.disc_button_colors.foreground_disabled = 0x80FFFFFF;
  fullscreen_scheme.disc_button_colors.background = 0xCC2B3E48;
  fullscreen_scheme.disc_button_colors.background_hover = 0xCC536B77;
  fullscreen_scheme.disc_button_colors.background_down = 0xCC96AFBB;

  fullscreen_scheme.normal_factor = 0.0f;
  fullscreen_scheme.incognito_factor = 0.0f;
  fullscreen_scheme.fullscreen_factor = 1.0f;

  g_incognito_scheme.Get() = normal_scheme;
  ColorScheme& incognito_scheme = g_incognito_scheme.Get();
  incognito_scheme.world_background = 0xFF2E2E2E;
  incognito_scheme.floor = 0xFF282828;
  incognito_scheme.ceiling = 0xFF2F2F2F;
  incognito_scheme.floor_grid = 0xCC595959;

  incognito_scheme.disc_button_colors.foreground = 0x80FFFFFF;
  incognito_scheme.disc_button_colors.foreground_disabled = 0x33E6E6E6;
  incognito_scheme.disc_button_colors.background = 0xCC2B3E48;
  incognito_scheme.disc_button_colors.background_hover = 0xCC505050;
  incognito_scheme.disc_button_colors.background_down = 0xCC888888;

  incognito_scheme.prompt_foreground = 0xCCFFFFFF;
  incognito_scheme.prompt_primary_button_colors.foreground = 0xD9000000;
  incognito_scheme.prompt_primary_button_colors.foreground_disabled =
      0xD9000000;
  incognito_scheme.prompt_primary_button_colors.background = 0xD9FFFFFF;
  incognito_scheme.prompt_primary_button_colors.background_hover = 0xFF8C8C8C;
  incognito_scheme.prompt_primary_button_colors.background_down = 0xE6FFFFFF;
  incognito_scheme.prompt_secondary_button_colors.foreground = 0xD9000000;
  incognito_scheme.prompt_secondary_button_colors.foreground_disabled =
      0xD9000000;
  incognito_scheme.prompt_secondary_button_colors.background = 0x80FFFFFF;
  incognito_scheme.prompt_secondary_button_colors.background_hover = 0xFF8C8C8C;
  incognito_scheme.prompt_secondary_button_colors.background_down = 0xE6FFFFFF;

  incognito_scheme.url_bar_background = 0xFF454545;
  incognito_scheme.url_bar_separator = MakeWhite(12);
  incognito_scheme.url_bar_text = MakeWhite(65);
  incognito_scheme.url_bar_hint_text = MakeWhite(50);
  incognito_scheme.url_bar_dangerous_icon = SK_ColorWHITE;
  incognito_scheme.url_bar_button.background_hover = MakeWhite(8);
  incognito_scheme.url_bar_button.background_down = MakeWhite(8);
  incognito_scheme.url_bar_button.foreground = MakeWhite(65);
  incognito_scheme.url_bar_button.foreground_disabled = MakeWhite(24);
  incognito_scheme.url_text_emphasized = MakeWhite(80);
  incognito_scheme.url_text_deemphasized = MakeWhite(30);
  incognito_scheme.menu_text = MakeWhite(87);
  incognito_scheme.omnibox_background = incognito_scheme.url_bar_background;
  incognito_scheme.omnibox_text_selection.foreground =
      incognito_scheme.url_bar_text;
  incognito_scheme.omnibox_text_selection.background = MakeWhite(8);

  incognito_scheme.normal_factor = 0.0f;
  incognito_scheme.incognito_factor = 1.0f;
  incognito_scheme.fullscreen_factor = 0.0f;

  initialized = true;
}

static constexpr size_t kButtonColorsSize = 20;

}  // namespace

ColorScheme::ColorScheme() = default;
ColorScheme::ColorScheme(const ColorScheme& other) = default;
ColorScheme& ColorScheme::operator=(const ColorScheme& other) = default;

static_assert(kButtonColorsSize == sizeof(ButtonColors),
              "If the new colors are added to ButtonColors, we must explicitly "
              "bump this size and update operator== below");

bool ButtonColors::operator==(const ButtonColors& other) const {
  return background == other.background &&
         background_hover == other.background_hover &&
         background_down == other.background_down &&
         foreground == other.foreground &&
         foreground_disabled == other.foreground_disabled;
}

bool ButtonColors::operator!=(const ButtonColors& other) const {
  return !(*this == other);
}

SkColor ButtonColors::GetBackgroundColor(bool hovered, bool pressed) const {
  if (pressed)
    return background_down;
  if (hovered)
    return background_hover;
  return background;
}

SkColor ButtonColors::GetForegroundColor(bool disabled) const {
  return disabled ? foreground_disabled : foreground;
}

bool TextSelectionColors::operator==(const TextSelectionColors& other) const {
  return cursor == other.cursor && background == other.background &&
         foreground == other.foreground;
}

bool TextSelectionColors::operator!=(const TextSelectionColors& other) const {
  return !(*this == other);
}

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  if (mode == kModeIncognito)
    return g_incognito_scheme.Get();
  if (mode == kModeFullscreen)
    return g_fullscreen_scheme.Get();
  return g_normal_scheme.Get();
}

void ColorScheme::UpdateForComponent(const base::Version& component_version) {
  if (component_version >= AssetsLoader::MinVersionWithGradients()) {
    ColorScheme& normal_scheme = g_normal_scheme.Get();
    normal_scheme.disc_button_colors.foreground = 0xA6000000;
    normal_scheme.disc_button_colors.foreground_disabled = 0x33000000;
    normal_scheme.disc_button_colors.background = 0xFFEEEEEE;
    normal_scheme.disc_button_colors.background_hover = SK_ColorWHITE;
    normal_scheme.modal_prompt_secondary_button_colors.foreground_disabled =
        normal_scheme.disc_button_colors.foreground_disabled;
    normal_scheme.modal_prompt_secondary_button_colors.background =
        normal_scheme.modal_prompt_background;
    normal_scheme.url_bar_background = MakeColor(0xEEEEEE, 87);
    normal_scheme.omnibox_background = MakeColor(0xEEEEEE, 100);

    ColorScheme& incognito_scheme = g_incognito_scheme.Get();
    incognito_scheme.disc_button_colors.background = MakeColor(0x263238, 100);
    incognito_scheme.disc_button_colors.background_hover = 0xCC404A50;
    incognito_scheme.disc_button_colors.background_down = 0xCC212B31;
    incognito_scheme.url_bar_background = MakeColor(0x263238, 87);
    incognito_scheme.omnibox_background = MakeColor(0x263238, 100);
  }
}

}  // namespace vr
