// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/renderer/accessibility/read_anything_app_model.h"

#include <cstddef>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/renderer/accessibility/read_aloud_traversal_utils.h"
#include "chrome/renderer/accessibility/read_anything_node_utils.h"
#include "content/public/renderer/render_thread.h"
#include "services/strings/grit/services_strings.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/accessibility/ax_tree_update_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

base::TimeDelta kTimeElapsedSincePageLoadForDataCollectionSeconds =
    base::Seconds(30);
base::TimeDelta kTimeElapsedSinceTreeChangedForDataCollectionSeconds =
    base::Seconds(10);

bool GetIsGoogleDocs(const GURL& url) {
  // A Google Docs URL is in the form of "https://docs.google.com/document*" or
  // "https://docs.sandbox.google.com/document*".
  constexpr const char* kDocsURLDomain[] = {"docs.google.com",
                                            "docs.sandbox.google.com"};
  if (url.SchemeIsHTTPOrHTTPS()) {
    for (const std::string& google_docs_url : kDocsURLDomain) {
      if (url.DomainIs(google_docs_url) && url.has_path() &&
          url.path().starts_with("/document") &&
          !url.ExtractFileName().empty()) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace

ReadAnythingAppModel::ReadAnythingAppModel() {
  // We default to true since base_language_code_ is en by default and that
  // supports all these fonts.
  for (const auto* font : fonts::kReadAnythingFonts) {
    supported_fonts_[font] = true;
  }
}

ReadAnythingAppModel::~ReadAnythingAppModel() = default;

ReadAnythingAppModel::AXTreeInfo::AXTreeInfo(
    std::unique_ptr<ui::AXTreeManager> other) {
  manager = std::move(other);
}

ReadAnythingAppModel::AXTreeInfo::~AXTreeInfo() = default;

void ReadAnythingAppModel::OnSettingsRestoredFromPrefs(
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing,
    const std::string& font,
    double font_size,
    bool links_enabled,
    bool images_enabled,
    read_anything::mojom::Colors color) {
  line_spacing_ = static_cast<size_t>(line_spacing);
  letter_spacing_ = static_cast<size_t>(letter_spacing);
  font_name_ = font;
  font_size_ = font_size;
  links_enabled_ = links_enabled;
  images_enabled_ = images_enabled;
  color_theme_ = static_cast<size_t>(color);
}

void ReadAnythingAppModel::InsertDisplayNode(const ui::AXNodeID& node) {
  display_node_ids_.insert(node);
}

void ReadAnythingAppModel::InsertSelectionNode(const ui::AXNodeID& node) {
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
  // If the previous selection was non-empty and outside the distilled content,
  // we will always redraw either a) the new selected content or b) the original
  // distilled content if the new selection is inside that or if the selection
  // was cleared.
  bool need_to_draw = !selection_from_action_ && !SelectionInsideDisplayNodes();
  // Save the current selection
  UpdateSelection();

  if (has_selection_ && was_empty) {
    base::UmaHistogramEnumeration(
        string_constants::kEmptyStateHistogramName,
        ReadAnythingEmptyState::kSelectionAfterEmptyStateShown);
    tree_infos_.at(active_tree_id_)->num_selections++;
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

  if (!start_node || !end_node) {
    return;
  }

  // If start node or end node is invisible or ignored, the selection was
  // invalid.
  if (start_node->IsInvisibleOrIgnored() || end_node->IsInvisibleOrIgnored()) {
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
    if (!a11y::IsNodeIgnoredForReadAnything(GetAXNode(ancestor_id), is_pdf_)) {
      InsertSelectionNode(ancestor_id);
    }
  }

  // Find the parent of the start and end nodes so we can look at nearby sibling
  // nodes. Since the start and end nodes might be in different section of the
  // tree, get the parents for start and end separately. Otherwise, the end
  // selection might not render.
  ui::AXNode* start_parent = a11y::GetParentForSelection(start_node);
  ui::AXNode* end_parent = a11y::GetParentForSelection(end_node);

  // If either parent is missing, selection is invalid and we should return
  // early.
  if (start_parent == nullptr || end_parent == nullptr) {
    return;
  }

  ui::AXNode* first_sibling_node =
      start_parent->GetFirstUnignoredChildCrossingTreeBoundary();
  ui::AXNode* deepest_last_descendant =
      end_parent->GetDeepestLastUnignoredDescendantCrossingTreeBoundary();

  // If the last sibling node is null, selection is invalid and we should
  // return early.
  if (deepest_last_descendant == nullptr) {
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
         first_sibling_node->CompareTo(*deepest_last_descendant).value_or(1) <=
             0) {
    if (!a11y::IsNodeIgnoredForReadAnything(GetAXNode(first_sibling_node->id()),
                                            is_pdf_)) {
      InsertSelectionNode(first_sibling_node->id());
    }

    first_sibling_node = first_sibling_node->GetNextUnignoredInTreeOrder();
  }
}

bool ReadAnythingAppModel::ContentNodesOnlyContainHeadings() {
  for (ui::AXNodeID node_id : content_node_ids_) {
    ui::AXNode* node = GetAXNode(node_id);
    if (!node || node->IsInvisibleOrIgnored() ||
        node->GetRole() == ax::mojom::Role::kHeading) {
      continue;
    }

    // Check the ancestors for a heading node, as inline text boxes or static
    // text nodes could be deeply nested under one.
    base::queue<ui::AXNode*> ancestors =
        node->GetAncestorsCrossingTreeBoundaryAsQueue();
    bool found_heading = false;
    while (!ancestors.empty()) {
      if (ancestors.front()->GetRole() == ax::mojom::Role::kHeading) {
        found_heading = true;
        break;
      }
      ancestors.pop();
    }
    if (!found_heading) {
      return false;
    }
  }
  return true;
}

void ReadAnythingAppModel::ComputeDisplayNodeIdsForDistilledTree() {
  DCHECK(!content_node_ids_.empty());

  // RM should not display just headings, return early to allow "highlight to
  // use RM" empty state screen to show.
  // TODO(crbug.com/40802192): Remove when Screen2x doesn't return just
  // headings.
  if (features::IsReadAnythingWithAlgorithmEnabled() &&
      ContentNodesOnlyContainHeadings()) {
    return;
  }

  // Clear the map to store new expanded states.
  aria_expanded_node_states_.clear();

  // Display nodes are the nodes which will be displayed by the rendering
  // algorithm of Read Anything app.ts. We wish to create a subtree which
  // stretches down from tree root to every content node and includes the
  // descendants of each content node.
  for (auto content_node_id : content_node_ids_) {
    ui::AXNode* content_node = GetAXNode(content_node_id);
    // TODO(crbug.com/40802192): If content_node_id is from a child tree of the
    // active ax tree, GetAXNode will return nullptr. Fix GetAXNode to harvest
    // nodes from child trees, and then replace the `if (!content_node)` check
    // with `DCHECK(content_node)`.
    // TODO(abigailbklein) This prevents the crash in crbug.com/1402788, but may
    // not be the correct approach. Do we need a version of
    // GetDeepestLastUnignoredDescendant() that works on ignored nodes?
    if (!content_node || content_node->IsInvisibleOrIgnored()) {
      continue;
    }

    // Ignore aria-expanded for editables.
    if (content_node->HasHtmlAttribute("aria-expanded") &&
        !content_node->HasState(ax::mojom::State::kRichlyEditable)) {
      // Capture the expanded state. ARIA expanded is not supported by all
      // element types, but gmail for example uses it anyways. Check the
      // attribute directly for that reason.
      const std::string& aria_expanded_state =
          content_node->GetHtmlAttribute("aria-expanded");
      aria_expanded_node_states_[content_node_id] = aria_expanded_state;
      // Don't include collapsed aria-expanded items.
      if (aria_expanded_state != "true") {
        continue;
      }
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
      // For certain PDFs, the ancestor may not be in the same tree. Ignore if
      // so.
      ui::AXNode* ancestor_node = GetAXNode(ancestor_id);
      if (ancestor_node &&
          !a11y::IsNodeIgnoredForReadAnything(ancestor_node, is_pdf_)) {
        InsertDisplayNode(ancestor_id);
      }
    }

    // Add all descendant ids to the set.
    ui::AXNode* next_node = content_node;
    ui::AXNode* deepest_last_descendant =
        content_node->GetDeepestLastUnignoredDescendant();
    if (!deepest_last_descendant) {
      continue;
    }
    while (next_node != deepest_last_descendant) {
      next_node = next_node->GetNextUnignoredInTreeOrder();
      if (!a11y::IsNodeIgnoredForReadAnything(GetAXNode(next_node->id()),
                                              is_pdf_)) {
        InsertDisplayNode(next_node->id());
      }
    }
  }
}

bool ReadAnythingAppModel::IsCurrentSelectionEmpty() {
  return (start_node_id_ != ui::kInvalidAXNodeID) &&
         (end_node_id_ != ui::kInvalidAXNodeID) &&
         (start_node_id_ == end_node_id_) && (start_offset_ == end_offset_);
}

bool ReadAnythingAppModel::SelectionInsideDisplayNodes() {
  return IsCurrentSelectionEmpty() ||
         (base::Contains(display_node_ids_, start_node_id_) &&
          base::Contains(display_node_ids_, end_node_id_));
}

ui::AXSerializableTree* ReadAnythingAppModel::GetTreeFromId(
    const ui::AXTreeID& tree_id) const {
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  DCHECK(ContainsTree(tree_id));
  return static_cast<ui::AXSerializableTree*>(
      tree_infos_.at(tree_id)->manager->ax_tree());
}

bool ReadAnythingAppModel::ContainsTree(const ui::AXTreeID& tree_id) const {
  return base::Contains(tree_infos_, tree_id);
}

void ReadAnythingAppModel::AddTree(
    const ui::AXTreeID& tree_id,
    std::unique_ptr<ui::AXSerializableTree> tree) {
  DCHECK(!ContainsTree(tree_id));

  for (auto& observer : observers_) {
    observer.OnTreeAdded(tree.get());
  }

  std::unique_ptr<ui::AXTreeManager> manager =
      std::make_unique<ui::AXTreeManager>(std::move(tree));
  std::unique_ptr<ReadAnythingAppModel::AXTreeInfo> tree_info =
      std::make_unique<AXTreeInfo>(std::move(manager));
  tree_infos_[tree_id] = std::move(tree_info);
}

void ReadAnythingAppModel::EraseTree(const ui::AXTreeID& tree_id) {
  auto it = tree_infos_.find(tree_id);
  if (it == tree_infos_.end()) {
    return;
  }
  ui::AXTree* ax_tree = it->second->manager->ax_tree();
  for (auto& observer : observers_) {
    observer.OnTreeRemoved(ax_tree);
  }

  tree_infos_.erase(it);

  // Ensure any pending updates associated with the erased tree are removed.
  pending_updates_map_.erase(tree_id);
}

void ReadAnythingAppModel::AddUrlInformationForTreeId(
    const ui::AXTreeID& tree_id) {
  // If the tree isn't yet created, do nothing.
  if (!ContainsTree(tree_id)) {
    return;
  }
  ReadAnythingAppModel::AXTreeInfo* tree_info = tree_infos_.at(tree_id).get();
  DCHECK(tree_info);

  // If the url information has already been set for this tree, do nothing.
  if (tree_info->is_url_information_set) {
    return;
  }

  DCHECK(tree_info->manager);
  // If the tree manager is not the root manager, do nothing.
  if (!tree_info->manager->IsRoot()) {
    return;
  }

  // If the tree doesn't have a root, or the root doesn't have a url set, do
  // nothing.
  ui::AXNode* root = tree_info->manager->GetRoot();
  if (!root || !root->HasStringAttribute(ax::mojom::StringAttribute::kUrl)) {
    return;
  }

  GURL url = GURL(root->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  tree_info->is_url_information_set = true;
  tree_info->is_docs = GetIsGoogleDocs(url);
}

bool ReadAnythingAppModel::IsDocs() const {
  // Sometimes during an initial page load, this may be called before the
  // tree has been initialized. If this happens, IsDocs should return false
  // instead of crashing.
  if (!tree_infos_.contains(active_tree_id_)) {
    return false;
  }

  return tree_infos_.at(active_tree_id_)->is_docs;
}

void ReadAnythingAppModel::AddPendingUpdates(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate>& updates) {
  std::vector<ui::AXTreeUpdate>& update = pending_updates_map_[tree_id];
  for (auto& item : updates) {
    update.emplace_back(std::move(item));
  }
}

void ReadAnythingAppModel::ClearPendingUpdates() {
  pending_updates_map_.clear();
}

void ReadAnythingAppModel::UnserializePendingUpdates(
    const ui::AXTreeID& tree_id) {
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
    std::vector<ui::AXTreeUpdate>& updates,
    const ui::AXTreeID& tree_id) {
  if (updates.empty()) {
    return;
  }
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  DCHECK(base::Contains(tree_infos_, tree_id));
  ui::AXSerializableTree* tree = GetTreeFromId(tree_id);
  size_t prev_tree_size = tree->size();
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

  AddUrlInformationForTreeId(tree_id);
  ProcessGeneratedEvents(event_generator, prev_tree_size, tree->size());
}

void ReadAnythingAppModel::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate>& updates,
    std::vector<ui::AXEvent>& events,
    const bool speech_playing) {
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
    if (distillation_in_progress_ || speech_playing) {
      AddPendingUpdates(tree_id, updates);
      ProcessNonGeneratedEvents(events);
      if (timer_since_tree_changed_for_data_collection_.IsRunning()) {
        CHECK(features::IsDataCollectionModeForScreen2xEnabled());
        timer_since_tree_changed_for_data_collection_.Reset();
      }
      return;
    } else {
      // We need to unserialize old updates before we can unserialize the new
      // ones.
      UnserializePendingUpdates(tree_id);
    }
    UnserializeUpdates(updates, tree_id);
    ProcessNonGeneratedEvents(events);
  } else {
    UnserializeUpdates(updates, tree_id);
  }
}

void ReadAnythingAppModel::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  // OnAXTreeDestroyed is called whenever the AXActionHandler in the browser
  // learns that an AXTree was destroyed. This could be from any tab, not just
  // the active one; therefore many tree_ids will not be found in
  // tree_infos_.
  if (!ContainsTree(tree_id)) {
    return;
  }
  if (active_tree_id_ == tree_id) {
    // TODO(crbug.com/40802192): If distillation is in progress, cancel the
    // distillation request.
    active_tree_id_ = ui::AXTreeIDUnknown();
    SetUkmSourceId(ukm::kInvalidSourceId);
  }
  EraseTree(tree_id);
}

const ukm::SourceId& ReadAnythingAppModel::UkmSourceId() {
  if (base::Contains(tree_infos_, active_tree_id_)) {
    ReadAnythingAppModel::AXTreeInfo* tree_info =
        tree_infos_.at(active_tree_id_).get();
    if (tree_info) {
      return tree_info->ukm_source_id;
    }
  }
  return ukm::kInvalidSourceId;
}

void ReadAnythingAppModel::SetUkmSourceId(const ukm::SourceId ukm_source_id) {
  if (!base::Contains(tree_infos_, active_tree_id_)) {
    return;
  }
  ReadAnythingAppModel::AXTreeInfo* tree_info =
      tree_infos_.at(active_tree_id_).get();
  if (!tree_info) {
    return;
  }
  if (tree_info->ukm_source_id == ukm::kInvalidSourceId) {
    tree_info->ukm_source_id = ukm_source_id;
  } else {
    DCHECK_EQ(tree_info->ukm_source_id, ukm_source_id);
  }
}

int32_t ReadAnythingAppModel::NumSelections() {
  if (base::Contains(tree_infos_, active_tree_id_)) {
    ReadAnythingAppModel::AXTreeInfo* tree_info =
        tree_infos_.at(active_tree_id_).get();
    if (tree_info) {
      return tree_info->num_selections;
    }
  }
  return 0;
}

void ReadAnythingAppModel::SetNumSelections(const int32_t& num_selections) {
  if (!base::Contains(tree_infos_, active_tree_id_)) {
    return;
  }
  ReadAnythingAppModel::AXTreeInfo* tree_info =
      tree_infos_.at(active_tree_id_).get();
  if (!tree_info) {
    return;
  }
  tree_info->num_selections = num_selections;
}

ui::AXNode* ReadAnythingAppModel::GetAXNode(
    const ui::AXNodeID& ax_node_id) const {
  ui::AXSerializableTree* tree = GetTreeFromId(active_tree_id_);
  return tree->GetFromId(ax_node_id);
}

bool ReadAnythingAppModel::NodeIsContentNode(
    const ui::AXNodeID& ax_node_id) const {
  return base::Contains(content_node_ids_, ax_node_id);
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

std::map<ui::AXTreeID, std::unique_ptr<ReadAnythingAppModel::AXTreeInfo>>*
ReadAnythingAppModel::GetTreesForTesting() {
  return &tree_infos_;
}

void ReadAnythingAppModel::EraseTreeForTesting(const ui::AXTreeID& tree_id) {
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

void ReadAnythingAppModel::OnSelection(ax::mojom::EventFrom event_from) {
  // If event_from is kUser, the user selected text on the main web page.
  // If event_from is kAction, the user selected text in RM and the main web
  // page was updated with that selection.
  // Edgecases:
  // 1. For selections in PDFs coming from the main pane or from the side
  // panel, event_from is set to kNone.
  // 2. When the user clicks and drags the cursor to highlight text on a
  // webpage, such that the anchor node and offset stays the same and the focus
  // node and/or offset changes, the first few selection events have event_from
  // kUser, but the subsequent selection events have event_from kPage. This is
  // the way UserActivationState is implemented. To detect this case, compare
  // the new selection to the saved selection. If the anchor is the same, update
  // the selection in RM.
  bool is_click_and_drag_selection = false;
  if (ContainsTree(active_tree_id_)) {
    ui::AXSelection selection =
        GetTreeFromId(active_tree_id_)->GetUnignoredSelection();
    is_click_and_drag_selection =
        (selection.anchor_object_id == start_node_id_ &&
         selection.anchor_offset == start_offset_ &&
         (selection.focus_object_id != end_node_id_ ||
          selection.focus_offset != end_offset_)) ||
        (selection.anchor_object_id == end_node_id_ &&
         selection.anchor_offset == end_offset_ &&
         (selection.focus_object_id != start_node_id_ ||
          selection.focus_offset != start_offset_));
  }
  if (event_from == ax::mojom::EventFrom::kUser ||
      event_from == ax::mojom::EventFrom::kAction ||
      (event_from == ax::mojom::EventFrom::kPage &&
       is_click_and_drag_selection) ||
      is_pdf_) {
    requires_post_process_selection_ = true;
    selection_from_action_ = event_from == ax::mojom::EventFrom::kAction;
  }
}

void ReadAnythingAppModel::SetActiveTreeId(const ui::AXTreeID& active_tree_id) {
  active_tree_id_ = active_tree_id;
  // If data collection mode for screen2x is enabled, begin
  // `timer_since_page_load_for_data_collection_` from here. This is a
  // one-shot timer which times 30 seconds from when the active AXTree changes.
  // This is one of two timers associated with the data collection flow. When
  // either of these timers expires, this triggers the screen2x distillation
  // data collection flow.
  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    timer_since_page_load_for_data_collection_.Start(
        FROM_HERE, kTimeElapsedSincePageLoadForDataCollectionSeconds,
        base::BindOnce(
            &ReadAnythingAppModel::SetPageFinishedLoadingForDataCollection,
            weak_ptr_factory_.GetWeakPtr(), true));
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
        page_finished_loading_ = true;
        // If data collection mode for screen2x is enabled, begin
        // `timer_since_tree_changed_for_data_collection_` from here. This is a
        // repeating one-shot timer which times 10 seconds from page load and
        // resets every time the accessibility tree changes. This is one of two
        // timers associated with the data collection flow. When either of these
        // timers expires, this triggers the screen2x distillation data
        // collection flow.
        if (features::IsDataCollectionModeForScreen2xEnabled()) {
          timer_since_tree_changed_for_data_collection_.Start(
              FROM_HERE, kTimeElapsedSinceTreeChangedForDataCollectionSeconds,
              base::BindRepeating(&ReadAnythingAppModel::
                                      SetPageFinishedLoadingForDataCollection,
                                  weak_ptr_factory_.GetWeakPtr(), true));
        }

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
        break;
      case ax::mojom::Event::kValueChanged:
        reset_draw_timer_ = true;
        break;
      case ax::mojom::Event::kAriaAttributeChangedDeprecated:
      case ax::mojom::Event::kMenuListValueChangedDeprecated:
        NOTREACHED();
    }
  }
}

void ReadAnythingAppModel::ProcessGeneratedEvents(
    const ui::AXEventGenerator& event_generator,
    size_t prev_tree_size,
    size_t tree_size) {
  // Note that this list of events may overlap with non-generated events in the
  // It's up to the consumer to pick but its generally good to prefer generated.
  for (const auto& event : event_generator) {
    switch (event.event_params->event) {
      case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED:
        OnSelection(event.event_params->event_from);
        break;
      case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      case ui::AXEventGenerator::Event::ALERT:
        requires_distillation_ = true;
        break;
      case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
        OnScroll(event.event_params->event_from_action ==
                     ax::mojom::Action::kSetSelection,
                 /* from_reading_mode= */ false);
        break;
      case ui::AXEventGenerator::Event::SUBTREE_CREATED:
        // PDFs are not completely loaded on the kLoadComplete event. The PDF
        // accessibility tree is only complete when the embedded node in the
        // tree is populated with the actual contents of the PDF. When this
        // happens, a SUBTREE_CREATED event will be generated and distillation
        // should occur.
        // However, when the user scrolls in the PDF, SUBTREE_CREATED events
        // will be generated. This happens because the accessibility tree tracks
        // the scroll position of the PDF (which part of the PDF is currently
        // displaying). To avoid distilling and causing RM to flicker, only
        // distill if the size of the updated tree is larger than before (to
        // capture the complete PDF load mentioned earlier).
        if (is_pdf_ && prev_tree_size < tree_size) {
          requires_distillation_ = true;
        }
        break;
      // Audit these events e.g. to trigger distillation.
      case ui::AXEventGenerator::Event::NONE:
      case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
      case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      case ui::AXEventGenerator::Event::ARIA_CURRENT_CHANGED:
      case ui::AXEventGenerator::Event::ARIA_NOTIFICATIONS_POSTED:
      case ui::AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::ATOMIC_CHANGED:
      case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
      case ui::AXEventGenerator::Event::AUTOFILL_AVAILABILITY_CHANGED:
      case ui::AXEventGenerator::Event::BUSY_CHANGED:
      case ui::AXEventGenerator::Event::CARET_BOUNDS_CHANGED:
      case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      case ui::AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED:
      case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
      case ui::AXEventGenerator::Event::COLLAPSED:
      case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
      case ui::AXEventGenerator::Event::DETAILS_CHANGED:
      case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
      case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
      case ui::AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
      case ui::AXEventGenerator::Event::ENABLED_CHANGED:
      case ui::AXEventGenerator::Event::EXPANDED:
      case ui::AXEventGenerator::Event::FOCUS_CHANGED:
      case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
      case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
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
        break;
      case ui::AXEventGenerator::Event::NAME_CHANGED:
        // TODO(francisjp): Determine if this logic should be specific to gmail.
        if (last_expanded_node_id_ == event.node_id) {
          ResetSelection();
          requires_post_process_selection_ = false;
          reset_last_expanded_node_id();
          redraw_required_ = true;
        }
        break;
      case ui::AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::ORIENTATION_CHANGED:
      case ui::AXEventGenerator::Event::PARENT_CHANGED:
      case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
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
      case ui::AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
      case ui::AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      case ui::AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
        break;
    }
  }
}

bool ReadAnythingAppModel::ScreenAIServiceReadyForDataColletion() const {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  return ScreenAIServiceReadyForDataColletion_;
}

void ReadAnythingAppModel::SetScreenAIServiceReadyForDataColletion(bool value) {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  ScreenAIServiceReadyForDataColletion_ = value;
  MaybeRunDataCollectionForScreen2xCallback();
}

bool ReadAnythingAppModel::PageFinishedLoadingForDataCollection() const {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  return PageFinishedLoadingForDataCollection_;
}

void ReadAnythingAppModel::SetPageFinishedLoadingForDataCollection(bool value) {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  PageFinishedLoadingForDataCollection_ = value;
  timer_since_page_load_for_data_collection_.Stop();
  timer_since_tree_changed_for_data_collection_.Stop();
  MaybeRunDataCollectionForScreen2xCallback();
}

void ReadAnythingAppModel::SetDataCollectionForScreen2xCallback(
    base::RepeatingCallback<void()> callback) {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  data_collection_for_screen2x_callback_ = std::move(callback);
}

void ReadAnythingAppModel::MaybeRunDataCollectionForScreen2xCallback() {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  if (PageFinishedLoadingForDataCollection_ &&
      ScreenAIServiceReadyForDataColletion_) {
    CHECK(data_collection_for_screen2x_callback_);
    data_collection_for_screen2x_callback_.Run();
  }
}

void ReadAnythingAppModel::IncreaseTextSize() {
  font_size_ += kReadAnythingFontScaleIncrement;
  if (font_size_ > kReadAnythingMaximumFontScale) {
    font_size_ = kReadAnythingMaximumFontScale;
  }
}

void ReadAnythingAppModel::DecreaseTextSize() {
  font_size_ -= kReadAnythingFontScaleIncrement;
  if (font_size_ < kReadAnythingMinimumFontScale) {
    font_size_ = kReadAnythingMinimumFontScale;
  }
}

void ReadAnythingAppModel::ResetTextSize() {
  font_size_ = kReadAnythingDefaultFontScale;
}

void ReadAnythingAppModel::ToggleLinksEnabled() {
  links_enabled_ = !links_enabled_;
}

void ReadAnythingAppModel::ToggleImagesEnabled() {
  images_enabled_ = !images_enabled_;
}

void ReadAnythingAppModel::SetBaseLanguageCode(const std::string& code) {
  DCHECK(!code.empty());
  base_language_code_ = code;
  // Update whether each font is supported by the new language code.
  for (const auto& [font, font_info] : fonts::kFontInfos) {
    if (font_info.num_langs_supported > 0) {
      supported_fonts_[font] =
          (std::find(font_info.langs_supported,
                     font_info.langs_supported + font_info.num_langs_supported,
                     code) !=
           font_info.langs_supported + font_info.num_langs_supported);
    }
  }
}

std::vector<std::string> ReadAnythingAppModel::GetSupportedFonts() {
  std::vector<std::string> font_choices_;
  for (const auto* font : fonts::kReadAnythingFonts) {
    if (supported_fonts_[font]) {
      font_choices_.emplace_back(font);
    }
  }
  return font_choices_;
}

void ReadAnythingAppModel::AddObserver(ModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingAppModel::RemoveObserver(ModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
