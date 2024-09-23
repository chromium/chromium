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
  SkColor web_vr_background;
  SkColor web_vr_floor_center;
  SkColor web_vr_floor_edge;
  SkColor web_vr_floor_grid;

  // Specific element background and foregrounds
  SkColor modal_prompt_icon_foreground;
  SkColor modal_prompt_background;
  SkColor modal_prompt_foreground;

  SkColor web_vr_timeout_spinner;
  SkColor web_vr_timeout_message_background;
  SkColor web_vr_timeout_message_foreground;

  SkColor webvr_permission_background;
  SkColor webvr_permission_foreground;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_
