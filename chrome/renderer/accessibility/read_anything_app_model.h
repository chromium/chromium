// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "ui/accessibility/ax_selection.h"

namespace ui {
class AXNode;
}  // namespace ui

// A class that holds state for the ReadAnythingAppController for the Read
// Anything WebUI app.
class ReadAnythingAppModel {
 public:
  ReadAnythingAppModel();
  ~ReadAnythingAppModel();
  ReadAnythingAppModel(const ReadAnythingAppModel& other) = delete;
  ReadAnythingAppModel& operator=(const ReadAnythingAppModel&) = delete;

  // Theme
  const std::string& font_name() const { return font_name_; }
  float font_size() const { return font_size_; }
  float letter_spacing() const { return letter_spacing_; }
  float line_spacing() const { return line_spacing_; }
  const SkColor& foreground_color() const { return foreground_color_; }
  const SkColor& background_color() const { return background_color_; }

  // Selection.
  bool has_selection() const { return has_selection_; }
  ui::AXNodeID start_node_id() const { return start_node_id_; }
  ui::AXNodeID end_node_id() const { return end_node_id_; }
  int32_t start_offset() const { return start_offset_; }
  int32_t end_offset() const { return end_offset_; }

  void SetStart(ui::AXNodeID start_node_id, int32_t start_offset);
  void SetEnd(ui::AXNodeID end_node_id, int32_t end_offset);

  void OnThemeChanged(read_anything::mojom::ReadAnythingThemePtr new_theme);

  void Reset();
  void ResetSelection(ui::AXSelection selection);

 private:
  double GetLetterSpacingValue(
      read_anything::mojom::LetterSpacing letter_spacing) const;
  double GetLineSpacingValue(
      read_anything::mojom::LineSpacing line_spacing) const;

  // Theme information.
  std::string font_name_ = kReadAnythingDefaultFontName;
  float font_size_ = kReadAnythingDefaultFontScale;
  float letter_spacing_ =
      (int)read_anything::mojom::LetterSpacing::kDefaultValue;
  float line_spacing_ = (int)read_anything::mojom::LineSpacing::kDefaultValue;
  SkColor background_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  SkColor foreground_color_ = (int)read_anything::mojom::Colors::kDefaultValue;

  // Selection information.
  bool has_selection_ = false;
  ui::AXNodeID start_node_id_ = ui::kInvalidAXNodeID;
  ui::AXNodeID end_node_id_ = ui::kInvalidAXNodeID;
  int32_t start_offset_ = -1;
  int32_t end_offset_ = -1;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
