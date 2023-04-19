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

bool ReadAnythingAppModel::PostProcessSelection() {
  DCHECK_NE(active_tree_id_, ui::AXTreeIDUnknown());
  DCHECK(ContainsTree(active_tree_id_));

  // If the previous selection was inside the distilled content, that means we
  // are currently displaying the distilled content in Read Anything. We may not
  // need to redraw the distilled content if the user's new selection is inside
  // the distilled content.
  // If the previous selection was outside the distilled content, we will always
  // redraw either a) the new selected content or b) the original distilled
  // content if the new selection is inside that or if the selection was
  // cleared.
  bool need_to_draw = !SelectionInsideDisplayNodes();

  // Save the current selection
  UpdateSelection();

  // If the main panel selection contains content outside of the distilled
  // content, we need to find the selected nodes to display instead of the
  // distilled content.
  if (has_selection_ && !SelectionInsideDisplayNodes()) {
    ComputeSelectionNodeIds();
    return true;
  }

  return need_to_draw;
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

void ReadAnythingAppModel::ComputeSelectionNodeIds() {
  DCHECK(has_selection_);
  DCHECK_NE(active_tree_id_, ui::AXTreeIDUnknown());
  DCHECK(ContainsTree(active_tree_id_));

  ui::AXNode* start_node = GetAXNode(start_node_id_);
  DCHECK(start_node);
  ui::AXNode* end_node = GetAXNode(end_node_id_);
  DCHECK(end_node);

  // If start node or end node is ignored, the selection was invalid.
  if (start_node->IsIgnored() || end_node->IsIgnored()) {
    return;
  }

  // Selection nodes are the nodes which will be displayed by the rendering
  // algorithm of Read Anything app.ts if there is a selection that contains
  // content outside of the distilled content. We wish to create a subtree which
  // stretches from start node to end node with tree root as the root.

  // Add all ancestor ids of start node, including the start node itself. This
  // does a first walk down to start node.
  base::queue<ui::AXNode*> ancestors =
      start_node->GetAncestorsCrossingTreeBoundaryAsQueue();
  while (!ancestors.empty()) {
    ui::AXNodeID ancestor_id = ancestors.front()->id();
    ancestors.pop();
    if (!IsNodeIgnoredForReadAnything(ancestor_id)) {
      InsertSelectionNode(ancestor_id);
    }
  }

  // Do a pre-order walk of the tree from the start node to the end node and add
  // all nodes to the list.
  // TODO(crbug.com/1266555): Right now, we are going from start node to an
  // unignored node that is before or equal to the end node. This condition was
  // changed from next_node != end node because when a paragraph is selected
  // with a triple click, we sometimes pass the end node, causing a SEGV_ACCERR.
  // We need to investigate this case in more depth.
  ui::AXNode* next_node = start_node->GetNextUnignoredInTreeOrder();
  while (next_node && next_node->CompareTo(*end_node) <= 0) {
    if (!IsNodeIgnoredForReadAnything(next_node->id())) {
      InsertSelectionNode(next_node->id());
    }
    next_node = next_node->GetNextUnignoredInTreeOrder();
  }
}

void ReadAnythingAppModel::ComputeDisplayNodeIdsForDistilledTree() {
  DCHECK(!content_node_ids_.empty());

  // Display nodes are the nodes which will be displayed by the rendering
  // algorithm of Read Anything app.ts. We wish to create a subtree which
  // stretches down from tree root to every content node and includes the
  // descendants of each content node.
  for (auto content_node_id : content_node_ids_) {
    ui::AXNode* content_node = GetAXNode(content_node_id);
    // TODO(crbug.com/1266555): If content_node_id is from a child tree of the
    // active ax tree, GetAXNode will return nullptr. Fix GetAXNode to harvest
    // nodes from child trees, and then replace the `if (!content_node)` check
    // with `DCHECK(content_node)`.
    // TODO(abigailbklein) This prevents the crash in crbug.com/1402788, but may
    // not be the correct approach. Do we need a version of
    // GetDeepestLastUnignoredChild() that works on ignored nodes?
    if (!content_node || content_node->IsIgnored()) {
      continue;
    }

    // Add all ancestor ids, including the content node itself, which is the
    // first ancestor in the queue. Exit the loop early if an ancestor is
    // already in display_node_ids(); this means that all of the
    // remaining ancestors in the queue are also already in display_node_ids.
    // IsNodeIgnoredForReadAnything removes control nodes from display_node_ids,
    // which is used by GetChildren(). This effectively prunes the tree at the
    // control node. For example, a button and its static text inside will be
    // removed.
    base::queue<ui::AXNode*> ancestors =
        content_node->GetAncestorsCrossingTreeBoundaryAsQueue();
    while (!ancestors.empty()) {
      ui::AXNodeID ancestor_id = ancestors.front()->id();
      if (base::Contains(display_node_ids_, ancestor_id)) {
        break;
      }
      ancestors.pop();
      if (!IsNodeIgnoredForReadAnything(ancestor_id)) {
        InsertDisplayNode(ancestor_id);
      }
    }

    // Add all descendant ids to the set.
    ui::AXNode* next_node = content_node;
    ui::AXNode* deepest_last_child =
        content_node->GetDeepestLastUnignoredChild();
    if (!deepest_last_child) {
      continue;
    }
    while (next_node != deepest_last_child) {
      next_node = next_node->GetNextUnignoredInTreeOrder();
      if (!IsNodeIgnoredForReadAnything(next_node->id())) {
        InsertDisplayNode(next_node->id());
      }
    }
  }
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

void ReadAnythingAppModel::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  // OnAXTreeDestroyed is called whenever the AXActionHandler in the browser
  // learns that an AXTree was destroyed. This could be from any tab, not just
  // the active one; therefore many tree_ids will not be found in trees_.
  if (!ContainsTree(tree_id)) {
    return;
  }
  if (active_tree_id_ == tree_id) {
    // TODO(crbug.com/1266555): If distillation is in progress, cancel the
    // distillation request.
    SetActiveTreeId(ui::AXTreeIDUnknown());
    SetActiveUkmSourceId(ukm::kInvalidSourceId);
  }
  EraseTree(tree_id);
}

ui::AXNode* ReadAnythingAppModel::GetAXNode(ui::AXNodeID ax_node_id) const {
  ui::AXSerializableTree* tree = GetTreeFromId(active_tree_id_).get();
  return tree->GetFromId(ax_node_id);
}

bool ReadAnythingAppModel::IsNodeIgnoredForReadAnything(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);
  // Ignore interactive elements, except for text fields.
  ax::mojom::Role role = ax_node->GetRole();
  return (ui::IsControl(role) && !ui::IsTextField(role)) || ui::IsSelect(role);
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

std::map<ui::AXTreeID, std::vector<ui::AXTreeUpdate>>&
ReadAnythingAppModel::GetPendingUpdatesForTesting() {
  return pending_updates_map_;
}

std::map<ui::AXTreeID, std::unique_ptr<ui::AXSerializableTree>>*
ReadAnythingAppModel::GetTreesForTesting() {
  return &trees_;
}

void ReadAnythingAppModel::EraseTreeForTesting(ui::AXTreeID tree_id) {
  EraseTree(tree_id);
}
