// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_model.h"

void ReadAnythingAppModel::OnThemeChanged(
    read_anything::mojom::ReadAnythingThemePtr new_theme) {
  font_name_ = new_theme->font_name;
  font_size_ = new_theme->font_size;
  letter_spacing_ = GetLetterSpacingValue(new_theme->letter_spacing);
  line_spacing_ = GetLineSpacingValue(new_theme->line_spacing);
  background_color_ = new_theme->background_color;
  foreground_color_ = new_theme->foreground_color;
}

double ReadAnythingAppModel::GetLetterSpacingValue(
    read_anything::mojom::LetterSpacing letter_spacing) const {
  switch (letter_spacing) {
    case read_anything::mojom::LetterSpacing::kTightDeprecated:
      return -0.05;
    case read_anything::mojom::LetterSpacing::kStandard:
      return 0;
    case read_anything::mojom::LetterSpacing::kWide:
      return 0.05;
    case read_anything::mojom::LetterSpacing::kVeryWide:
      return 0.1;
  }
}

double ReadAnythingAppModel::GetLineSpacingValue(
    read_anything::mojom::LineSpacing line_spacing) const {
  switch (line_spacing) {
    case read_anything::mojom::LineSpacing::kTightDeprecated:
      return 1.0;
    case read_anything::mojom::LineSpacing::kStandard:
      return 1.15;
    case read_anything::mojom::LineSpacing::kLoose:
      return 1.5;
    case read_anything::mojom::LineSpacing::kVeryLoose:
      return 2.0;
  }
}
