// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/color_scheme.h"

#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "ui/gfx/color_palette.h"

namespace vr {

namespace {

base::LazyInstance<ColorScheme>::Leaky g_normal_scheme =
    LAZY_INSTANCE_INITIALIZER;

void InitializeColorSchemes() {
  static bool initialized = false;
  if (initialized)
    return;

  ColorScheme& normal_scheme = g_normal_scheme.Get();
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
