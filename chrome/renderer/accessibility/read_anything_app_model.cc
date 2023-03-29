// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_model.h"

#include "base/containers/contains.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_update_util.h"

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

void ReadAnythingAppModel::InsertSelectionNode(ui::AXNodeID node) {
  selection_node_ids_.insert(node);
}

void ReadAnythingAppModel::Reset(
    const std::vector<ui::AXNodeID>& content_node_ids) {
  content_node_ids_ = content_node_ids;
  display_node_ids_.clear();
  distillation_in_progress_ = false;
  ResetSelection();
}

void ReadAnythingAppModel::ResetSelection() {
  selection_node_ids_.clear();
  start_node_id_ = ui::kInvalidAXNodeID;
  end_node_id_ = ui::kInvalidAXNodeID;
  start_offset_ = -1;
  end_offset_ = -1;
  has_selection_ = false;
}

void ReadAnythingAppModel::UpdateSelection() {
  ResetSelection();
  ui::AXSelection selection =
      GetTreeFromId(active_tree_id_)->GetUnignoredSelection();
  has_selection_ = selection.anchor_object_id != ui::kInvalidAXNodeID &&
                   selection.focus_object_id != ui::kInvalidAXNodeID &&
                   !selection.IsCollapsed();
  if (!has_selection_) {
    return;
  }

  // Identify the start and end node ids and offsets. The start node comes
  // earlier than end node in the tree order. We need to send the selection to
  // JS in forward order. If they are sent as backward selections, JS will
  // collapse the selection so no selection will be rendered in Read Anything.
  start_node_id_ = selection.is_backward ? selection.focus_object_id
                                         : selection.anchor_object_id;
  end_node_id_ = selection.is_backward ? selection.anchor_object_id
                                       : selection.focus_object_id;
  start_offset_ =
      selection.is_backward ? selection.focus_offset : selection.anchor_offset;
  end_offset_ =
      selection.is_backward ? selection.anchor_offset : selection.focus_offset;
}

bool ReadAnythingAppModel::SelectionInsideDisplayNodes() {
  return base::Contains(display_node_ids_, start_node_id_) &&
         base::Contains(display_node_ids_, end_node_id_);
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

  // Ensure any pending updates associated with the erased tree are removed.
  pending_updates_map_.erase(tree_id);
}

void ReadAnythingAppModel::AddPendingUpdates(
    const ui::AXTreeID tree_id,
    const std::vector<ui::AXTreeUpdate>& updates) {
  std::vector<ui::AXTreeUpdate> update = GetOrCreatePendingUpdateAt(tree_id);
  update.insert(update.end(), std::make_move_iterator(updates.begin()),
                std::make_move_iterator(updates.end()));
  pending_updates_map_[tree_id] = update;
}

void ReadAnythingAppModel::ClearPendingUpdates() {
  pending_updates_map_.clear();
}

void ReadAnythingAppModel::UnserializePendingUpdates(ui::AXTreeID tree_id) {
  if (!pending_updates_map_.contains(tree_id)) {
    return;
  }
  // TODO(b/1266555): Ensure there are no crashes / unexpected behavior if
  //  an accessibility event is received on the same tree after unserialization
  //  has begun.
  std::vector<ui::AXTreeUpdate> update =
      pending_updates_map_.extract(tree_id).mapped();
  DCHECK(update.empty() || tree_id == active_tree_id_);
  UnserializeUpdates(update, tree_id);
}

void ReadAnythingAppModel::UnserializeUpdates(
    const std::vector<ui::AXTreeUpdate>& updates,
    const ui::AXTreeID& tree_id) {
  if (updates.empty()) {
    return;
  }
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  DCHECK(base::Contains(trees_, tree_id));
  ui::AXSerializableTree* tree = trees_[tree_id].get();
  DCHECK(tree);
  // Try to merge updates. If the updates are mergeable, MergeAXTreeUpdates will
  // return true and merge_updates_out will contain the updates. Otherwise, if
  // the updates are not mergeable, merge_updates_out will be empty.
  const std::vector<ui::AXTreeUpdate>* merged_updates = &updates;
  std::vector<ui::AXTreeUpdate> merge_updates_out;
  if (ui::MergeAXTreeUpdates(updates, &merge_updates_out)) {
    merged_updates = &merge_updates_out;
  }

  // Unserialize the updates.
  for (const ui::AXTreeUpdate& update : *merged_updates) {
    tree->Unserialize(update);
  }
}

void ReadAnythingAppModel::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    ui::AXTreeObserver* tree_observer) {
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  // Create a new tree if an event is received for a tree that is not yet in
  // the tree list.
  if (!ContainsTree(tree_id)) {
    std::unique_ptr<ui::AXSerializableTree> new_tree =
        std::make_unique<ui::AXSerializableTree>();
    new_tree->AddObserver(tree_observer);
    AddTree(tree_id, std::move(new_tree));
  }
  // If a tree update on the active tree is received while distillation is in
  // progress, cache updates that are received but do not yet unserialize them.
  // Drawing must be done on the same tree that was sent to the distiller,
  // so it’s critical that updates are not unserialized until drawing is
  // complete.
  if (tree_id == active_tree_id_) {
    if (distillation_in_progress_) {
      AddPendingUpdates(tree_id, updates);
      return;
    } else {
      // We need to unserialize old updates before we can unserialize the new
      // ones
      UnserializePendingUpdates(tree_id);
    }
  }
  UnserializeUpdates(std::move(updates), tree_id);
}

ui::AXNode* ReadAnythingAppModel::GetAXNode(ui::AXNodeID ax_node_id) const {
  ui::AXSerializableTree* tree = GetTreeFromId(active_tree_id_).get();
  return tree->GetFromId(ax_node_id);
}

bool ReadAnythingAppModel::IsNodeIgnoredForReadAnything(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  // Ignore interactive elements.
  ax::mojom::Role role = ax_node->GetRole();
  return ui::IsControl(role) || ui::IsSelect(role);
}

bool ReadAnythingAppModel::NodeIsContentNode(ui::AXNodeID ax_node_id) const {
  return base::Contains(content_node_ids_, ax_node_id);
}

const std::vector<ui::AXTreeUpdate>&
ReadAnythingAppModel::GetOrCreatePendingUpdateAt(ui::AXTreeID tree_id) {
  if (!pending_updates_map_.contains(tree_id)) {
    pending_updates_map_[tree_id] = std::vector<ui::AXTreeUpdate>();
  }

  return pending_updates_map_[tree_id];
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
