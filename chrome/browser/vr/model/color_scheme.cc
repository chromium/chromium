// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/color_scheme.h"

#include <type_traits>

#include "base/check_op.h"
#include "ui/gfx/color_palette.h"

namespace vr {

const ColorScheme& ColorScheme::GetColorScheme() {
  static_assert(std::is_trivially_destructible<ColorScheme>::value);
  static const ColorScheme normal_scheme([] {
    ColorScheme normal_scheme;
    normal_scheme.web_vr_background = SK_ColorBLACK;
    normal_scheme.web_vr_floor_edge = SK_ColorBLACK;
    normal_scheme.web_vr_floor_center = 0xD9212121;
    normal_scheme.web_vr_floor_grid = 0xD9212121;

    normal_scheme.modal_prompt_icon_foreground = 0xFF4285F4;
    normal_scheme.modal_prompt_background = 0xFFF5F5F5;
    normal_scheme.modal_prompt_foreground = 0xFF333333;

    normal_scheme.web_vr_timeout_spinner = 0xFFF3F3F3;
    normal_scheme.web_vr_timeout_message_background = 0xFF444444;
    normal_scheme.web_vr_timeout_message_foreground =
        normal_scheme.web_vr_timeout_spinner;

    normal_scheme.webvr_permission_background = 0xD9212121;
    normal_scheme.webvr_permission_foreground = SK_ColorWHITE;
    return normal_scheme;
  }());
  return normal_scheme;
}

ColorScheme::ColorScheme() = default;
ColorScheme::ColorScheme(const ColorScheme& other) = default;
ColorScheme& ColorScheme::operator=(const ColorScheme& other) = default;

}  // namespace vr
