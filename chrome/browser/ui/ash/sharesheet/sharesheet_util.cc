// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"

#include <string>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace sharesheet {

std::unique_ptr<views::Label> CreateShareLabel(
    const std::u16string& text,
    const int text_context,
    const int line_height,
    const SkColor color,
    const gfx::HorizontalAlignment alignment,
    const int text_style) {
  auto label = std::make_unique<views::Label>(text, text_context, text_style);
  label->SetLineHeight(line_height);
  label->SetEnabledColor(color);
  label->SetHorizontalAlignment(alignment);
  return label;
}

std::unique_ptr<views::Label> CreateShareLabel(
    const std::u16string& text,
    const TypographyToken style,
    const ui::ColorId color_id,
    const gfx::HorizontalAlignment alignment) {
  auto label = std::make_unique<views::Label>(text);
  bubble_utils::ApplyStyle(label.get(), style, color_id);
  label->SetHorizontalAlignment(alignment);
  return label;
}

}  // namespace sharesheet
}  // namespace ash
