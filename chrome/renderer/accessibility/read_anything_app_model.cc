// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_model.h"
#include <cstddef>
#include <string>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_update_util.h"

ReadAnythingAppModel::ReadAnythingAppModel() {
  // TODO(crbug.com/1450930): Use a global ukm recorder instance instead.
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
}

ReadAnythingAppModel::~ReadAnythingAppModel() {
  SetActiveUkmSourceId(ukm::kInvalidSourceId);
}

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
  requires_post_process_selection_ = false;
  selection_from_action_ = false;
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

  bool was_empty = is_empty();
  requires_post_process_selection_ = false;

  // If the new selection came from the side panel, we don't need to draw
  // anything in the side panel, since whatever was being selected had to have
  // been drawn already.
  // If the previous selection was inside the distilled content, that means we
  // are currently displaying the distilled content in Read Anything. We may not
  // need to redraw the distilled content if the user's new selection is inside
  // the distilled content.
  // If the previous selection was outside the distilled content, we will always
  // redraw either a) the new selected content or b) the original distilled
  // content if the new selection is inside that or if the selection was
  // cleared.
  bool need_to_draw = !selection_from_action_ && !SelectionInsideDisplayNodes();

  // Save the current selection
  UpdateSelection();

  if (has_selection_ && was_empty) {
    base::UmaHistogramEnumeration(
        string_constants::kEmptyStateHistogramName,
        ReadAnythingEmptyState::kSelectionAfterEmptyStateShown);
    num_selections_++;
  }

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

  // Find the parent of the start and end nodes so we can look at nearby sibling
  // nodes. Since the start and end nodes might be in different section of the
  // tree, get the parents for start and end separately. Otherwise, the end
  // selection might not render.
  ui::AXNode* start_parent = GetParentForSelection(start_node);
  ui::AXNode* end_parent = GetParentForSelection(end_node);

  // If either parent is missing, selection is invalid and we should return
  // early.
  if (start_parent == nullptr || end_parent == nullptr) {
    return;
  }

  ui::AXNode* first_sibling_node =
      start_parent->GetFirstUnignoredChildCrossingTreeBoundary();
  ui::AXNode* last_sibling_node =
      end_parent->GetDeepestLastUnignoredChildCrossingTreeBoundary();

  // If the last sibling node is null, selection is invalid and we should
  // return early.
  if (last_sibling_node == nullptr) {
    return;
  }

  // TODO(b/1266555): Consider using ax_position.h here to better manage
  // selection.
  // Traverse the tree from and including the first sibling node to the last
  // last sibling node, inclusive. This ensures that when select-to-distill
  // is used to distill non-distillable content (such as Gmail), text
  // outside of the selected portion but on the same line is still
  // distilled, even if there's special formatting.
  while (first_sibling_node &&
         first_sibling_node->CompareTo(*last_sibling_node).value_or(1) <= 0) {
    if (!IsNodeIgnoredForReadAnything(first_sibling_node->id())) {
      InsertSelectionNode(first_sibling_node->id());
    }

    first_sibling_node = first_sibling_node->GetNextUnignoredInTreeOrder();
  }
}

ui::AXNode* ReadAnythingAppModel::GetParentForSelection(ui::AXNode* node) {
  ui::AXNode* parent = node->GetUnignoredParentCrossingTreeBoundary();
  // For most nodes, the parent is the same as the most direct parent. However,
  // to handle special types of text formatting such as links and custom spans,
  // another parent may be needed. e.g. when a link is highlighted, the start
  // node has an "inline" display but the parent we want would have a "block"
  // display role, so in order to get the common parent of
  // all sibling nodes, the grandparent should be used.
  // Displays of type "list-item" is an exception to the "inline" display rule
  // so that all siblings in a list can be shown correctly to avoid
  //  misnumbering.
  while (parent && parent->GetUnignoredParentCrossingTreeBoundary() &&
         parent->HasStringAttribute(ax::mojom::StringAttribute::kDisplay) &&
         ((parent->GetStringAttribute(ax::mojom::StringAttribute::kDisplay)
               .find("inline") != std::string::npos) ||
          (parent->GetStringAttribute(ax::mojom::StringAttribute::kDisplay)
               .find("list-item") != std::string::npos))) {
    parent = parent->GetUnignoredParentCrossingTreeBoundary();
  }

  return parent;
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
  CHECK(tree);
  // Try to merge updates. If the updates are mergeable, MergeAXTreeUpdates will
  // return true and merge_updates_out will contain the updates. Otherwise, if
  // the updates are not mergeable, merge_updates_out will be empty.
  const std::vector<ui::AXTreeUpdate>* merged_updates = &updates;
  std::vector<ui::AXTreeUpdate> merge_updates_out;
  if (ui::MergeAXTreeUpdates(updates, &merge_updates_out)) {
    merged_updates = &merge_updates_out;
  }

  // Build an event generator prior to any unserializations.
  ui::AXEventGenerator event_generator(tree);

  // Unserialize the updates.
  for (const ui::AXTreeUpdate& update : *merged_updates) {
    tree->Unserialize(update);
  }

  ProcessGeneratedEvents(event_generator);
}

void ReadAnythingAppModel::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const std::vector<ui::AXEvent>& events) {
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  // Create a new tree if an event is received for a tree that is not yet in
  // the tree list.
  if (!ContainsTree(tree_id)) {
    std::unique_ptr<ui::AXSerializableTree> new_tree =
        std::make_unique<ui::AXSerializableTree>();
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
      ProcessNonGeneratedEvents(events);
      return;
    } else {
      // We need to unserialize old updates before we can unserialize the new
      // ones.
      UnserializePendingUpdates(tree_id);
    }
    UnserializeUpdates(std::move(updates), tree_id);
    ProcessNonGeneratedEvents(events);
  } else {
    UnserializeUpdates(std::move(updates), tree_id);
  }
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

void ReadAnythingAppModel::SetActiveUkmSourceId(ukm::SourceId source_id) {
  // Record the number of selections made on the current page if it was not
  // distillable.
  if (active_ukm_source_id_ != ukm::kInvalidSourceId &&
      content_node_ids_.empty()) {
    ukm::builders::Accessibility_ReadAnything_EmptyState(active_ukm_source_id_)
        .SetTotalNumSelections(num_selections_)
        .Record(ukm_recorder_.get());
  }
  num_selections_ = 0;
  active_ukm_source_id_ = source_id;
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
      // This value needs to be at least 1.35 to avoid cutting off descenders
      // with the highlight with larger fonts such as Poppins.
      return 1.35;
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

void ReadAnythingAppModel::OnScroll(bool on_selection,
                                    bool from_reading_mode) const {
  if (on_selection) {
    // If the scroll event came from the side panel because of a selection, then
    // this means the main panel was selected, causing the side panel to scroll
    // & vice versa.
    base::UmaHistogramEnumeration(
        string_constants::kScrollEventHistogramName,
        from_reading_mode ? ReadAnythingScrollEvent::kSelectedMainPanel
                          : ReadAnythingScrollEvent::kSelectedSidePanel);
  } else {
    base::UmaHistogramEnumeration(
        string_constants::kScrollEventHistogramName,
        from_reading_mode ? ReadAnythingScrollEvent::kScrolledSidePanel
                          : ReadAnythingScrollEvent::kScrolledMainPanel);
  }
}

void ReadAnythingAppModel::ProcessNonGeneratedEvents(
    const std::vector<ui::AXEvent>& events) {
  // Note that this list of events may overlap with generated events in the
  // model. It's up to the consumer to pick but its generally good to prefer
  // generated. The consumer should not process the same event here and for
  // generated events.
  for (auto& event : events) {
    switch (event.event_type) {
      case ax::mojom::Event::kLoadComplete:
        requires_distillation_ = true;
        // TODO(accessibility): Some pages may never completely load; use a
        // timer with a reasonable delay to force distillation -> drawing.
        // Investigate if this is needed.
        break;

        // Audit these events e.g. to require distillation.
      case ax::mojom::Event::kActiveDescendantChanged:
      case ax::mojom::Event::kCheckedStateChanged:
      case ax::mojom::Event::kChildrenChanged:
      case ax::mojom::Event::kDocumentSelectionChanged:
      case ax::mojom::Event::kDocumentTitleChanged:
      case ax::mojom::Event::kExpandedChanged:
      case ax::mojom::Event::kRowCollapsed:
      case ax::mojom::Event::kRowCountChanged:
      case ax::mojom::Event::kRowExpanded:
      case ax::mojom::Event::kSelectedChildrenChanged:
      case ax::mojom::Event::kNone:
      case ax::mojom::Event::kAlert:
      case ax::mojom::Event::kAutocorrectionOccured:
      case ax::mojom::Event::kBlur:
      case ax::mojom::Event::kClicked:
      case ax::mojom::Event::kControlsChanged:
      case ax::mojom::Event::kEndOfTest:
      case ax::mojom::Event::kFocus:
      case ax::mojom::Event::kFocusAfterMenuClose:
      case ax::mojom::Event::kFocusContext:
      case ax::mojom::Event::kHide:
      case ax::mojom::Event::kHitTestResult:
      case ax::mojom::Event::kHover:
      case ax::mojom::Event::kImageFrameUpdated:
      case ax::mojom::Event::kLayoutComplete:
      case ax::mojom::Event::kLiveRegionCreated:
      case ax::mojom::Event::kLiveRegionChanged:
      case ax::mojom::Event::kLoadStart:
      case ax::mojom::Event::kLocationChanged:
      case ax::mojom::Event::kMediaStartedPlaying:
      case ax::mojom::Event::kMediaStoppedPlaying:
      case ax::mojom::Event::kMenuEnd:
      case ax::mojom::Event::kMenuListValueChanged:
      case ax::mojom::Event::kMenuPopupEnd:
      case ax::mojom::Event::kMenuPopupStart:
      case ax::mojom::Event::kMenuStart:
      case ax::mojom::Event::kMouseCanceled:
      case ax::mojom::Event::kMouseDragged:
      case ax::mojom::Event::kMouseMoved:
      case ax::mojom::Event::kMousePressed:
      case ax::mojom::Event::kMouseReleased:
      case ax::mojom::Event::kScrolledToAnchor:
      case ax::mojom::Event::kScrollPositionChanged:
      case ax::mojom::Event::kSelection:
      case ax::mojom::Event::kSelectionAdd:
      case ax::mojom::Event::kSelectionRemove:
      case ax::mojom::Event::kShow:
      case ax::mojom::Event::kStateChanged:
      case ax::mojom::Event::kTextChanged:
      case ax::mojom::Event::kWindowActivated:
      case ax::mojom::Event::kWindowDeactivated:
      case ax::mojom::Event::kWindowVisibilityChanged:
      case ax::mojom::Event::kTextSelectionChanged:
      case ax::mojom::Event::kTooltipClosed:
      case ax::mojom::Event::kTooltipOpened:
      case ax::mojom::Event::kTreeChanged:
      case ax::mojom::Event::kValueChanged:
        break;
      case ax::mojom::Event::kAriaAttributeChangedDeprecated:
        NOTREACHED_NORETURN();
    }
  }
}

void ReadAnythingAppModel::ProcessGeneratedEvents(
    const ui::AXEventGenerator& event_generator) {
  // Note that this list of events may overlap with non-generated events in the
  // It's up to the consumer to pick but its generally good to prefer generated.
  for (const auto& event : event_generator) {
    switch (event.event_params.event) {
      case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED:
        if (event.event_params.event_from == ax::mojom::EventFrom::kUser ||
            event.event_params.event_from == ax::mojom::EventFrom::kAction) {
          requires_post_process_selection_ = true;
          selection_from_action_ =
              event.event_params.event_from == ax::mojom::EventFrom::kAction;
        }
        break;
      case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      case ui::AXEventGenerator::Event::ALERT:
        requires_distillation_ = true;
        break;
      case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
        OnScroll(event.event_params.event_from_action ==
                     ax::mojom::Action::kSetSelection,
                 /* from_reading_mode= */ false);
        break;

      // Audit these events e.g. to trigger distillation.
      case ui::AXEventGenerator::Event::NONE:
      case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
      case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      case ui::AXEventGenerator::Event::ARIA_CURRENT_CHANGED:
      case ui::AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::ATOMIC_CHANGED:
      case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
      case ui::AXEventGenerator::Event::AUTOFILL_AVAILABILITY_CHANGED:
      case ui::AXEventGenerator::Event::BUSY_CHANGED:
      case ui::AXEventGenerator::Event::CARET_BOUNDS_CHANGED:
      case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      case ui::AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED:
      case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
      case ui::AXEventGenerator::Event::CLASS_NAME_CHANGED:
      case ui::AXEventGenerator::Event::COLLAPSED:
      case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
      case ui::AXEventGenerator::Event::DETAILS_CHANGED:
      case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
      case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
      case ui::AXEventGenerator::Event::DROPEFFECT_CHANGED:
      case ui::AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
      case ui::AXEventGenerator::Event::ENABLED_CHANGED:
      case ui::AXEventGenerator::Event::EXPANDED:
      case ui::AXEventGenerator::Event::FOCUS_CHANGED:
      case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
      case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
      case ui::AXEventGenerator::Event::GRABBED_CHANGED:
      case ui::AXEventGenerator::Event::HASPOPUP_CHANGED:
      case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
      case ui::AXEventGenerator::Event::IGNORED_CHANGED:
      case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
      case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
      case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
      case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
      case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
      case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED:
      case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
      case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
      case ui::AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
      case ui::AXEventGenerator::Event::LIVE_STATUS_CHANGED:
      case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
      case ui::AXEventGenerator::Event::MENU_POPUP_END:
      case ui::AXEventGenerator::Event::MENU_POPUP_START:
      case ui::AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
      case ui::AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
      case ui::AXEventGenerator::Event::NAME_CHANGED:
      case ui::AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::ORIENTATION_CHANGED:
      case ui::AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::PARENT_CHANGED:
      case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
      case ui::AXEventGenerator::Event::PORTAL_ACTIVATED:
      case ui::AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
      case ui::AXEventGenerator::Event::RANGE_VALUE_CHANGED:
      case ui::AXEventGenerator::Event::RANGE_VALUE_MAX_CHANGED:
      case ui::AXEventGenerator::Event::RANGE_VALUE_MIN_CHANGED:
      case ui::AXEventGenerator::Event::RANGE_VALUE_STEP_CHANGED:
      case ui::AXEventGenerator::Event::READONLY_CHANGED:
      case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
      case ui::AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
      case ui::AXEventGenerator::Event::ROLE_CHANGED:
      case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
      case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
      case ui::AXEventGenerator::Event::SELECTED_CHANGED:
      case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      case ui::AXEventGenerator::Event::SELECTED_VALUE_CHANGED:
      case ui::AXEventGenerator::Event::SET_SIZE_CHANGED:
      case ui::AXEventGenerator::Event::SORT_CHANGED:
      case ui::AXEventGenerator::Event::STATE_CHANGED:
      case ui::AXEventGenerator::Event::SUBTREE_CREATED:
      case ui::AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
      case ui::AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      case ui::AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
        break;
    }
  }
}
