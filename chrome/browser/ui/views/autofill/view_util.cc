// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/view_util.h"

#include <utility>

#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"

namespace autofill {

std::unique_ptr<views::Label> CreateLabelWithColorReadabilityDisabled(
    const base::string16& text,
    int text_context,
    int text_style) {
  auto label = std::make_unique<views::Label>(text, text_context, text_style);
  label->SetAutoColorReadabilityEnabled(false);
  // Forces the color for the required context and style to be applied. It may
  // have been overridden by the default theme's color before auto-color
  // readability was disabled.
  label->SetEnabledColor(
      views::style::GetColor(*label, text_context, text_style));
  return label;
}

}  // namespace autofill
