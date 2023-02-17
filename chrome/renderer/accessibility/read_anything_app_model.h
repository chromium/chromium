// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include "chrome/common/accessibility/read_anything.mojom.h"

// A class that holds state for the ReadAnythingAppController for the Read
// Anything WebUI app.
class ReadAnythingAppModel {
 public:
  ReadAnythingAppModel() = default;
  ReadAnythingAppModel(const ReadAnythingAppModel& other) = default;
  ReadAnythingAppModel& operator=(const ReadAnythingAppModel&) = delete;
  ~ReadAnythingAppModel() = default;

  // Theme
  const std::string& font_name() const { return font_name_; }
  float font_size() const { return font_size_; }
  float letter_spacing() const { return letter_spacing_; }
  float line_spacing() const { return line_spacing_; }
  const SkColor& foreground_color() const { return foreground_color_; }
  const SkColor& background_color() const { return background_color_; }

  void OnThemeChanged(read_anything::mojom::ReadAnythingThemePtr new_theme);

 private:
  double GetLetterSpacingValue(
      read_anything::mojom::LetterSpacing letter_spacing) const;
  double GetLineSpacingValue(
      read_anything::mojom::LineSpacing line_spacing) const;

  // Theme information.
  // TODO(crbug.com/c/1266555): Reference read_anything_constants to set
  // default values.
  std::string font_name_;
  float font_size_;
  float letter_spacing_ =
      (int)read_anything::mojom::LetterSpacing::kDefaultValue;
  float line_spacing_ = (int)read_anything::mojom::LineSpacing::kDefaultValue;
  SkColor background_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  SkColor foreground_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
