// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_model.h"

#include "base/containers/contains.h"

#include "ui/accessibility/ax_serializable_tree.h"

ReadAnythingAppModel::ReadAnythingAppModel() = default;
ReadAnythingAppModel::~ReadAnythingAppModel() = default;

void ReadAnythingAppModel::OnThemeChanged(
    read_anything::mojom::ReadAnythingThemePtr new_theme) {
  font_name_ = new_theme->font_name;
  font_size_ = new_theme->font_size;
  letter_spacing_ = GetLetterSpacingValue(new_theme->letter_spacing);
  line_spacing_ = GetLineSpacingValue(new_theme->line_spacing);
  background_color_ = new_theme->background_color;
  foreground_color_ = new_theme->foreground_color;
}

void ReadAnythingAppModel::InsertDisplayNode(ui::AXNodeID node) {
  display_node_ids_.insert(node);
}

void ReadAnythingAppModel::Reset(
    const std::vector<ui::AXNodeID>& content_node_ids) {
  content_node_ids_ = content_node_ids;
  display_node_ids_.clear();
  start_node_id_ = ui::kInvalidAXNodeID;
  end_node_id_ = ui::kInvalidAXNodeID;
  start_offset_ = -1;
  end_offset_ = -1;
  has_selection_ = false;
  distillation_in_progress_ = false;
}

void ReadAnythingAppModel::ResetSelection() {
  ui::AXSelection selection =
      GetTreeFromId(active_tree_id_)->GetUnignoredSelection();
  has_selection_ = selection.anchor_object_id != ui::kInvalidAXNodeID &&
                   selection.focus_object_id != ui::kInvalidAXNodeID;

  // Identify the start and end node ids and offsets. The start node comes
  // earlier than end node in the tree order.
  start_node_id_ = selection.is_backward ? selection.focus_object_id
                                         : selection.anchor_object_id;
  end_node_id_ = selection.is_backward ? selection.anchor_object_id
                                       : selection.focus_object_id;
  start_offset_ =
      selection.is_backward ? selection.focus_offset : selection.anchor_offset;
  end_offset_ =
      selection.is_backward ? selection.anchor_offset : selection.focus_offset;
}

void ReadAnythingAppModel::SetStart(ui::AXNodeID start_node_id,
                                    int32_t start_offset) {
  start_node_id_ = start_node_id;
  start_offset_ = start_offset;
}
void ReadAnythingAppModel::SetEnd(ui::AXNodeID end_node_id,
                                  int32_t end_offset) {
  end_node_id_ = end_node_id;
  end_offset_ = end_offset;
}

const std::unique_ptr<ui::AXSerializableTree>&
ReadAnythingAppModel::GetTreeFromId(ui::AXTreeID tree_id) const {
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  DCHECK(ContainsTree(tree_id));
  return trees_.at(tree_id);
}

bool ReadAnythingAppModel::ContainsTree(ui::AXTreeID tree_id) const {
  return base::Contains(trees_, tree_id);
}

void ReadAnythingAppModel::AddTree(
    ui::AXTreeID tree_id,
    std::unique_ptr<ui::AXSerializableTree> tree) {
  DCHECK(!ContainsTree(tree_id));
  trees_[tree_id] = std::move(tree);
}

void ReadAnythingAppModel::EraseTree(ui::AXTreeID tree_id) {
  trees_.erase(tree_id);
}

size_t ReadAnythingAppModel::NumTreesForTesting() const {
  return trees_.size();
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
