// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/color_scheme.h"

#include "base/check_op.h"
#include "base/lazy_instance.h"
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

base::LazyInstance<ColorScheme>::Leaky g_normal_scheme =
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
  normal_scheme.prompt_foreground = 0xCC000000;

  normal_scheme.url_bar_background = 0xCCB3B3B3;
  normal_scheme.url_bar_separator = MakeBlack(12);
  normal_scheme.url_bar_text = MakeBlack(65);
  normal_scheme.url_bar_hint_text = MakeBlack(50);
  normal_scheme.url_bar_dangerous_icon = gfx::kGoogleRed700;
  normal_scheme.url_text_emphasized = MakeBlack(80);
  normal_scheme.url_text_deemphasized = MakeBlack(30);
  normal_scheme.menu_text = MakeBlack(87);
  normal_scheme.omnibox_background = 0xFFEEEEEE;
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

  normal_scheme.controller_label_callout = SK_ColorWHITE;
  normal_scheme.controller_button = 0xFFEFEFEF;
  normal_scheme.controller_button_down = 0xFF2979FF;
  normal_scheme.controller_battery_full = 0xFFEFEFEF;
  normal_scheme.controller_battery_empty = 0xCCB3B3B3;

  normal_scheme.reposition_label = SK_ColorWHITE;
  normal_scheme.reposition_label_background = 0xAA333333;

  normal_scheme.content_reposition_frame = 0x66FFFFFF;

  normal_scheme.cursor_background_center = 0x66000000;
  normal_scheme.cursor_background_edge = SK_ColorTRANSPARENT;
  normal_scheme.cursor_foreground = SK_ColorWHITE;

  normal_scheme.webvr_permission_background = 0xD9212121;
  normal_scheme.webvr_permission_foreground = SK_ColorWHITE;

  initialized = true;
}

}  // namespace

ColorScheme::ColorScheme() = default;
ColorScheme::ColorScheme(const ColorScheme& other) = default;
ColorScheme& ColorScheme::operator=(const ColorScheme& other) = default;

const ColorScheme& ColorScheme::GetColorScheme() {
  InitializeColorSchemes();
  return g_normal_scheme.Get();
}

}  // namespace vr
