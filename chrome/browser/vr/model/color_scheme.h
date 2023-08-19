// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_
#define CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_

#include "base/version.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

struct VR_UI_EXPORT ColorScheme {
  static const ColorScheme& GetColorScheme();

  ColorScheme();
  ColorScheme(const ColorScheme& other);
  ColorScheme& operator=(const ColorScheme& other);

  // These colors should be named generically, if possible, so that they can be
  // meaningfully reused by multiple elements.
  SkColor world_background;
  SkColor floor;
  SkColor ceiling;
  SkColor floor_grid;
  SkColor web_vr_background;
  SkColor web_vr_floor_center;
  SkColor web_vr_floor_edge;
  SkColor web_vr_floor_grid;

  // Specific element background and foregrounds
  SkColor loading_indicator_foreground;
  SkColor loading_indicator_background;
  SkColor exit_warning_foreground;
  SkColor exit_warning_background;
  SkColor web_vr_transient_toast_foreground;
  SkColor web_vr_transient_toast_background;
  SkColor toast_foreground;
  SkColor toast_background;
  SkColor modal_prompt_icon_foreground;
  SkColor modal_prompt_background;
  SkColor modal_prompt_foreground;

  // The colors used for text and buttons on prompts.
  SkColor prompt_foreground;

  SkColor url_bar_background;
  SkColor url_bar_separator;
  SkColor url_bar_text;
  SkColor url_bar_hint_text;
  SkColor url_bar_dangerous_icon;
  SkColor url_text_emphasized;
  SkColor url_text_deemphasized;
  SkColor menu_text;
  SkColor omnibox_background;
  SkColor hyperlink;

  SkColor dimmer_outer;
  SkColor dimmer_inner;

  SkColor splash_screen_background;
  SkColor splash_screen_text_color;

  SkColor web_vr_timeout_spinner;
  SkColor web_vr_timeout_message_background;
  SkColor web_vr_timeout_message_foreground;

  SkColor speech_recognition_circle_background;

  SkColor snackbar_background;
  SkColor snackbar_foreground;

  SkColor controller_label_callout;
  SkColor controller_button;
  SkColor controller_button_down;
  SkColor controller_battery_full;
  SkColor controller_battery_empty;

  SkColor reposition_label;
  SkColor reposition_label_background;

  SkColor content_reposition_frame;

  SkColor cursor_background_center;
  SkColor cursor_background_edge;
  SkColor cursor_foreground;

  SkColor webvr_permission_background;
  SkColor webvr_permission_foreground;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_
