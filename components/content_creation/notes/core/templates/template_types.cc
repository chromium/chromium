// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_types.h"

namespace content_creation {

Background::Background(ARGBColor color) : color_(color) {}

TextStyle::TextStyle(const std::string& font_name,
                     ARGBColor font_color,
                     bool all_caps)
    : font_name_(font_name), font_color_(font_color), all_caps_(all_caps) {}

}  // namespace content_creation
