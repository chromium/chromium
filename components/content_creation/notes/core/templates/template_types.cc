// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_types.h"

namespace content_creation {

Background::Background(ARGBColor color) : color_(color) {}

TextStyle::TextStyle(const std::string& font_name,
                     ARGBColor font_color,
                     uint16_t weight,
                     bool all_caps,
                     TextAlignment alignment)
    : font_name_(font_name),
      font_color_(font_color),
      weight_(weight),
      all_caps_(all_caps),
      alignment_(alignment) {}

FooterStyle::FooterStyle(ARGBColor text_color, ARGBColor logo_color)
    : text_color_(text_color), logo_color_(logo_color) {}

}  // namespace content_creation
