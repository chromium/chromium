// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_UTIL_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_UTIL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_typography.h"
#include "ash/style/typography.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/text_constants.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
namespace sharesheet {

enum SharesheetViewID {
  SHARESHEET_BUBBLE_VIEW_ID = 1,
  HEADER_VIEW_ID,
  BODY_VIEW_ID,
  FOOTER_VIEW_ID,
  // ID for the view populated with targets that shows in the default
  // sharesheet.
  TARGETS_DEFAULT_VIEW_ID,
  TARGET_LABEL_VIEW_ID,
  HEADER_VIEW_TEXT_PREVIEW_ID,
  SHARE_ACTION_VIEW_ID,
};

std::unique_ptr<views::Label> CreateShareLabel(
    const std::u16string& text,
    const int text_context,
    const int line_height,
    const SkColor color,
    const gfx::HorizontalAlignment alignment,
    const int text_style = ash::STYLE_SHARESHEET);

std::unique_ptr<views::Label> CreateShareLabel(
    const std::u16string& text,
    const TypographyToken style,
    const ui::ColorId color_id,
    const gfx::HorizontalAlignment alignment);

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_UTIL_H_
