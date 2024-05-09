// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_model.h"

#include <cstddef>
#include <regex>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
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
#include "ui/accessibility/ax_tree_update_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

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

ReadAnythingAppModel::ReadAnythingAppModel() = default;

ReadAnythingAppModel::~ReadAnythingAppModel() = default;

ReadAnythingAppModel::AXTreeInfo::AXTreeInfo(
    std::unique_ptr<ui::AXTreeManager> other) {
  manager = std::move(other);
}

ReadAnythingAppModel::AXTreeInfo::~AXTreeInfo() = default;

ReadAnythingAppModel::ReadAloudCurrentGranularity::
    ReadAloudCurrentGranularity() {
  segments = std::map<ui::AXNodeID, ReadAloudTextSegment>();
}

ReadAnythingAppModel::ReadAloudCurrentGranularity::ReadAloudCurrentGranularity(
    const ReadAloudCurrentGranularity& other) = default;

ReadAnythingAppModel::ReadAloudCurrentGranularity::
    ~ReadAloudCurrentGranularity() = default;

void ReadAnythingAppModel::OnThemeChanged(
    read_anything::mojom::ReadAnythingThemePtr new_theme) {
  font_name_ = new_theme->font_name;
  font_size_ = new_theme->font_size;
  links_enabled_ = new_theme->links_enabled;
  letter_spacing_ = GetLetterSpacingValue(new_theme->letter_spacing);
  line_spacing_ = GetLineSpacingValue(new_theme->line_spacing);
  background_color_ = new_theme->background_color;
  foreground_color_ = new_theme->foreground_color;
}

void ReadAnythingAppModel::OnSettingsRestoredFromPrefs(
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing,
    const std::string& font,
    double font_size,
    bool links_enabled,
    read_anything::mojom::Colors color,
    double speech_rate,
    base::Value::Dict* voices,
    base::Value::List* languages_enabled_in_pref,
    read_anything::mojom::HighlightGranularity granularity) {
  line_spacing_ = GetLineSpacingValue(line_spacing);
  letter_spacing_ = GetLetterSpacingValue(letter_spacing);
  font_name_ = font;
  font_size_ = font_size;
  links_enabled_ = links_enabled;
  color_theme_ = static_cast<size_t>(color);
  speech_rate_ = speech_rate;
  voices_ = voices->Clone();
  languages_enabled_in_pref_ = languages_enabled_in_pref->Clone();
  highlight_granularity_ = static_cast<size_t>(granularity);
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
  ResetReadAloudState();
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
  bool need_to_draw = !selection_from_action_ && !NoCurrentSelection() &&
                      !SelectionInsideDisplayNodes();
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
      if (GetAXNode(ancestor_id) &&
          !IsNodeIgnoredForReadAnything(ancestor_id)) {
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
      if (!IsNodeIgnoredForReadAnything(next_node->id())) {
        InsertDisplayNode(next_node->id());
      }
    }
  }
}

bool ReadAnythingAppModel::NoCurrentSelection() {
  return start_node_id_ == end_node_id_ ||
         (start_node_id_ == ui::kInvalidAXNodeID &&
          end_node_id_ == ui::kInvalidAXNodeID);
}

bool ReadAnythingAppModel::SelectionInsideDisplayNodes() {
  return base::Contains(display_node_ids_, start_node_id_) &&
         base::Contains(display_node_ids_, end_node_id_);
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
  std::unique_ptr<ui::AXTreeManager> manager =
      std::make_unique<ui::AXTreeManager>(std::move(tree));
  std::unique_ptr<ReadAnythingAppModel::AXTreeInfo> tree_info =
      std::make_unique<AXTreeInfo>(std::move(manager));
  tree_infos_[tree_id] = std::move(tree_info);
}

void ReadAnythingAppModel::EraseTree(const ui::AXTreeID& tree_id) {
  tree_infos_.erase(tree_id);

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
    std::vector<ui::AXEvent>& events) {
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
    if (distillation_in_progress_ || speech_playing_) {
      AddPendingUpdates(tree_id, updates);
      ProcessNonGeneratedEvents(events);
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
    set_ukm_source_id(ukm::kInvalidSourceId);
  }
  EraseTree(tree_id);
}

const ukm::SourceId& ReadAnythingAppModel::ukm_source_id() {
  if (base::Contains(tree_infos_, active_tree_id_)) {
    ReadAnythingAppModel::AXTreeInfo* tree_info =
        tree_infos_.at(active_tree_id_).get();
    if (tree_info) {
      return tree_info->ukm_source_id;
    }
  }
  return ukm::kInvalidSourceId;
}

void ReadAnythingAppModel::set_ukm_source_id(
    const ukm::SourceId ukm_source_id) {
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

int32_t ReadAnythingAppModel::num_selections() {
  if (base::Contains(tree_infos_, active_tree_id_)) {
    ReadAnythingAppModel::AXTreeInfo* tree_info =
        tree_infos_.at(active_tree_id_).get();
    if (tree_info) {
      return tree_info->num_selections;
    }
  }
  return 0;
}

void ReadAnythingAppModel::set_num_selections(
    const int32_t& num_selections) {
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

bool ReadAnythingAppModel::IsNodeIgnoredForReadAnything(
    const ui::AXNodeID& ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  // If the node is not in the active tree (this could happen when RM is still
  // loading), ignore it.
  if (!ax_node) {
    return true;
  }
  ax::mojom::Role role = ax_node->GetRole();

  // PDFs processed with OCR have additional nodes that mark the start and end
  // of a page. The start of a page is indicated with a kBanner node that has a
  // child static text node. Ignore both. The end of a page is indicated with a
  // kContentInfo node that has a child static text node. Ignore the static text
  // node but keep the kContentInfo so a line break can be inserted in between
  // pages in GetHtmlTagForPDF.
  if (is_pdf_) {
    // The text content of the aforementioned kBanner or kContentInfo nodes is
    // the same as the text content of its child static text node.
    std::string text = ax_node->GetTextContentUTF8();
    ui::AXNode* parent = ax_node->GetParent();

    std::string pdf_begin_message =
        l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_BEGIN);
    std::string pdf_end_message =
        l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_END);

    bool is_start_or_end_static_text_node =
        parent && ((parent->GetRole() == ax::mojom::Role::kBanner &&
                    text == pdf_begin_message) ||
                   (parent->GetRole() == ax::mojom::Role::kContentInfo &&
                    text == pdf_end_message));
    if ((role == ax::mojom::Role::kBanner && text == pdf_begin_message) ||
        is_start_or_end_static_text_node) {
      return true;
    }
  }

  // Ignore interactive elements, except for text fields.
  return (ui::IsControl(role) && !ui::IsTextField(role)) || ui::IsSelect(role);
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
        if (features::IsDataCollectionModeForScreen2xEnabled()) {
          page_finished_loading_for_data_collection_ = true;
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
        if (event.event_from_action == ax::mojom::Action::kGetImageData) {
          image_to_update_node_id_ = event.id;
        }
        break;
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
      case ax::mojom::Event::kValueChanged:
        break;
      case ax::mojom::Event::kAriaAttributeChangedDeprecated:
      case ax::mojom::Event::kMenuListValueChangedDeprecated:
        NOTREACHED_NORETURN();
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
      case ui::AXEventGenerator::Event::NAME_CHANGED:
      case ui::AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::ORIENTATION_CHANGED:
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
      case ui::AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      case ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
      case ui::AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      case ui::AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
        break;
    }
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

// TODO: b/40275871 - make this more efficient as we are now calling this
// more often.
std::vector<std::string> ReadAnythingAppModel::GetSupportedFonts() const {
  std::vector<std::string> font_choices_;

  if (base::Contains(kLanguagesSupportedByPoppins, base_language_code())) {
    font_choices_.push_back("Poppins");
  }
  font_choices_.push_back("Sans-serif");
  font_choices_.push_back("Serif");
  if (base::Contains(kLanguagesSupportedByComicNeue, base_language_code())) {
    font_choices_.push_back("Comic Neue");
  }
  if (base::Contains(kLanguagesSupportedByLexendDeca, base_language_code())) {
    font_choices_.push_back("Lexend Deca");
  }
  if (base::Contains(kLanguagesSupportedByEbGaramond, base_language_code())) {
    font_choices_.push_back("EB Garamond");
  }
  if (base::Contains(kLanguagesSupportedByStixTwoText, base_language_code())) {
    font_choices_.push_back("STIX Two Text");
  }
  if (base::Contains(kLanguagesSupportedByAndika, base_language_code())) {
    font_choices_.push_back("Andika");
  }
  return font_choices_;
}

std::string ReadAnythingAppModel::GetHtmlTag(
    const ui::AXNodeID& ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  DCHECK(ax_node);

  std::string html_tag =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);

  if (is_pdf()) {
    return GetHtmlTagForPDF(ax_node, html_tag);
  }

  if (ui::IsTextField(ax_node->GetRole())) {
    return "div";
  }

  if (ui::IsHeading(ax_node->GetRole())) {
    int32_t hierarchical_level =
        ax_node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    if (hierarchical_level) {
      return std::format("h{}", hierarchical_level);
    }
  }

  if (html_tag == ui::ToString(ax::mojom::Role::kMark)) {
    // Replace mark element with bold element for readability.
    html_tag = "b";
  } else if (tree_infos_.at(active_tree_id_)->is_docs) {
    // Change HTML tags for SVG elements to allow Reading Mode to render text
    // for the Annotated Canvas elements in a Google Doc.
    if (html_tag == "svg") {
      html_tag = "div";
    }
    if (html_tag == "g" && ax_node->GetRole() == ax::mojom::Role::kParagraph) {
      html_tag = "p";
    }
  }

  return html_tag;
}

std::string ReadAnythingAppModel::GetAltText(
    const ui::AXNodeID& ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  CHECK(ax_node);
  std::string alt_text =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  return alt_text;
}

std::string ReadAnythingAppModel::GetImageDataUrl(
    const ui::AXNodeID& ax_node_id) const {
  ui::AXNode* ax_node = GetAXNode(ax_node_id);
  CHECK(ax_node);

  std::string url =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kImageDataUrl);
  return url;
}

std::string ReadAnythingAppModel::GetHtmlTagForPDF(
    ui::AXNode* ax_node,
    const std::string& html_tag) const {
  ax::mojom::Role role = ax_node->GetRole();

  // Some nodes in PDFs don't have an HTML tag so use role instead.
  switch (role) {
    case ax::mojom::Role::kEmbeddedObject:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kRootWebArea:
      return "span";
    case ax::mojom::Role::kParagraph:
      return "p";
    case ax::mojom::Role::kLink:
      return "a";
    case ax::mojom::Role::kStaticText:
      return "";
    case ax::mojom::Role::kHeading:
      return GetHeadingHtmlTagForPDF(ax_node, html_tag);
    // Add a line break after each page of an inaccessible PDF for readability
    // since there is no other formatting included in the OCR output.
    case ax::mojom::Role::kContentInfo:
      if (ax_node->GetTextContentUTF8() ==
          l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_END)) {
        return "br";
      }
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return html_tag.empty() ? "span" : html_tag;
  }
}

std::string ReadAnythingAppModel::GetHeadingHtmlTagForPDF(
    ui::AXNode* ax_node,
    const std::string& html_tag) const {
  // Sometimes whole paragraphs can be formatted as a heading. If the text is
  // longer than 2 lines, assume it was meant to be a paragragh,
  if (ax_node->GetTextContentUTF8().length() > (2 * kMaxLineWidth)) {
    return "p";
  }

  // A single block of text could be incorrectly formatted with multiple heading
  // nodes (one for each line of text) instead of a single paragraph node. This
  // case should be detected to improve readability. If there are multiple
  // consecutive nodes with the same heading level, assume that they are all a
  // part of one paragraph.
  ui::AXNode* next = ax_node->GetNextUnignoredSibling();
  ui::AXNode* prev = ax_node->GetPreviousUnignoredSibling();

  if ((next && next->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag) ==
                   html_tag) ||
      (prev && prev->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag) ==
                   html_tag)) {
    return "span";
  }

  int32_t hierarchical_level =
      ax_node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
  if (hierarchical_level) {
    return std::format("h{}", hierarchical_level);
  }
  return html_tag;
}

int ReadAnythingAppModel::GetNextSentence(const std::u16string& text) {
    std::u16string filtered_string(text);
    // When we receive text from a pdf node, there are return characters at each
    // visual line break in the page. If these aren't filtered before calling
    // GetNextGranularity on the text, text part of the same sentence will be
    // read as separate segments, which causes speech to sound choppy.
    // e.g. without filtering
    // 'This is a long sentence with \n\r a line break.'
    // will read and highlight "This is a long sentence with" and "a line break"
    // separately.
    if (is_pdf() && filtered_string.size() > 0) {
      size_t pos = filtered_string.find_first_of(u"\n\r");
      while (pos != std::string::npos && pos < filtered_string.size() - 2) {
        filtered_string.replace(pos, 1, u" ");
        pos = filtered_string.find_first_of(u"\n\r");
      }
  }
  return GetNextGranularity(filtered_string,
                            ax::mojom::TextBoundary::kSentenceStart);
}

int ReadAnythingAppModel::GetNextWord(const std::u16string& text) {
  return GetNextGranularity(text, ax::mojom::TextBoundary::kWordStart);
}

int ReadAnythingAppModel::GetNextGranularity(const std::u16string& text,
                                             ax::mojom::TextBoundary boundary) {
  // TODO(crbug.com/40927698): Investigate providing correct line breaks
  // or alternatively making adjustments to ax_text_utils to return boundaries
  // that minimize choppiness.
  std::vector<int> offsets;
  return ui::FindAccessibleTextBoundary(text, offsets, boundary, 0,
                                        ax::mojom::MoveDirection::kForward,
                                        ax::mojom::TextAffinity::kDefaultValue);
}

void ReadAnythingAppModel::InitAXPositionWithNode(
    const ui::AXNodeID& starting_node_id) {
  ui::AXNode* ax_node = GetAXNode(starting_node_id);

  // If instance is Null or Empty, create the next AxPosition
  if (ax_node != nullptr && (!ax_position_ || ax_position_->IsNullPosition())) {
    ax_position_ =
        ui::AXNodePosition::CreateTreePositionAtStartOfAnchor(*ax_node);
    current_text_index_ = 0;
    processed_granularity_index_ = 0;
    processed_granularities_on_current_page_.clear();
  }
}
void ReadAnythingAppModel::MovePositionToNextGranularity() {
  processed_granularity_index_++;
}

void ReadAnythingAppModel::MovePositionToPreviousGranularity() {
  if (processed_granularity_index_ > 0) {
    processed_granularity_index_--;
  }
}

std::vector<ui::AXNodeID> ReadAnythingAppModel::GetCurrentText() {
  while (processed_granularities_on_current_page_.size() <=
         processed_granularity_index_) {
    ReadAnythingAppModel::ReadAloudCurrentGranularity next_granularity =
        GetNextNodes();

    if (next_granularity.node_ids.size() == 0) {
      // TODO(crbug.com/40927698) think about behavior when increment happened
      // out of the content- should we reset the state?
      return next_granularity.node_ids;
    }

    processed_granularities_on_current_page_.push_back(next_granularity);
  }

  return processed_granularities_on_current_page_[processed_granularity_index_]
      .node_ids;
}
// TODO(crbug.com/40927698): Update to use AXRange to better handle multiple
// nodes. This may require updating GetText in ax_range.h to return AXNodeIds.
// AXRangeType#ExpandToEnclosingTextBoundary may also be useful.
ReadAnythingAppModel::ReadAloudCurrentGranularity
ReadAnythingAppModel::GetNextNodes() {
  ReadAnythingAppModel::ReadAloudCurrentGranularity current_granularity =
      ReadAnythingAppModel::ReadAloudCurrentGranularity();

  // Make sure we're adequately returning at the end of content.
  if (!ax_position_ || ax_position_->AtEndOfAXTree() ||
      ax_position_->IsNullPosition()) {
    return current_granularity;
  }

  // Loop through the tree in order to group nodes together into the same
  // granularity segment until there are no more pieces that can be added
  // to the current segment or we've reached the end of the tree.
  // e.g. if the following two nodes are next to one another in the tree:
  //  AXNode: id=1, text = "This is a "
  //  AXNode: id=2, text = "link. "
  // both AXNodes should be added to the current granularity, as the
  // combined text across the two nodes forms a complete sentence with sentence
  // granularity.
  // This allows text to be spoken smoothly across nodes with broken sentences,
  // such as links and formatted text.
  // TODO(crbug.com/40927698): Investigate how much of this can be pulled into
  // AXPosition to simplify Read Aloud-specific code and allow improvements
  // to be used by other places where AXPosition is used.
  while (!ax_position_->IsNullPosition() && !ax_position_->AtEndOfAXTree()) {
    ui::AXNode* anchor_node = GetNodeFromCurrentPosition();
    std::u16string text = anchor_node->GetTextContentUTF16();
    std::u16string text_substr = text.substr(current_text_index_);
    int prev_index = current_text_index_;
    // Gets the starting index for the next sentence in the current node.
    int next_sentence_index = GetNextSentence(text_substr) + prev_index;
    // If our current index within the current node is greater than that node's
    // text, look at the next node. If the starting index of the next sentence
    // in the node is the same the current index within the node, this means
    // that we've reached the end of all possible sentences within the current
    // node, and should move to the next node.
    if ((size_t)current_text_index_ >= text.size() ||
        (current_text_index_ == next_sentence_index)) {
      // Move the AXPosition to the next node.
      ax_position_ =
          GetNextValidPositionFromCurrentPosition(current_granularity);
      // Reset the current text index within the current node since we just
      // moved to a new node.
      current_text_index_ = 0;
      // If we've reached the end of the content, go ahead and return the
      // current list of nodes because there are no more nodes to look through.
      if (ax_position_->IsNullPosition() || ax_position_->AtEndOfAXTree() ||
          !ax_position_->GetAnchor()) {
        return current_granularity;
      }

      // If the position is now at the start of a paragraph and we already have
      // nodes to return, return the current list of nodes so that we don't
      // cross paragraph boundaries with text.
      if (ShouldSplitAtParagraph(ax_position_, current_granularity)) {
        return current_granularity;
      }

      std::u16string base_text =
          GetNodeFromCurrentPosition()->GetTextContentUTF16();

      // Look at the text of the items we've already added to the
      // current sentence (current_text) combined with the text of the next
      // node (base_text).
      const std::u16string& combined_text =
          current_granularity.text + base_text;
      // Get the index of the next sentence if we're looking at the combined
      // previous and current node text.
      int combined_sentence_index = GetNextSentence(combined_text);

      bool is_opening_punctuation = false;
      // The code that checks for accessible text boundaries sometimes
      // incorrectly includes opening punctuation (i.e. '(', '<', etc.) as part
      // of the prior sentence.
      // e.g. "This is a sentence.[2]" will return a sentence boundary for
      // "This is a sentence.[", splitting the opening and closing punctuation.
      // When opening punctuation is split like this in Read Aloud, text will
      // be read out for the punctuation e.g. "opening square bracket," which
      // we want to avoid.
      // Therefore, this is a workaround that prevents adding text from the
      // next node to the current segment if that text is a single character
      // and also opening punctuation. The opening punctuation will then be
      // read out as part of the next segment. If the opening punctuation is
      // followed by text and closing punctuation, the punctuation will not be
      // read out directly- just the text content.
      // TODO(crbug.com/40927698): See if it's possible to fix the code
      // in FindAccessibleTextBoundary instead so that this workaround isn't
      // needed.
      if (combined_sentence_index ==
          (int)current_granularity.text.length() + 1) {
        char c = combined_text[combined_sentence_index - 1];
        is_opening_punctuation = IsOpeningPunctuation(c);
      }

      // If the combined_sentence_index is the same as the current_text length,
      // the new node should not be considered part of the current sentence.
      // If these values differ, add the current node's text to the list of
      // nodes in the current sentence.
      // Consider these two examples:
      // Example 1:
      //  current text: Hello
      //  current node's text: , how are you?
      //    The current text length is 5, but the index of the next sentence of
      //    the combined text is 19, so the current node should be added to
      //    the current sentence.
      // Example 2:
      //  current text: Hello.
      //  current node: Goodbye.
      //    The current text length is 6, and the next sentence index of
      //    "Hello. Goodbye." is still 6, so the current node's text shouldn't
      //    be added to the current sentence.
      if (((int)current_granularity.text.length() < combined_sentence_index) &&
          !is_opening_punctuation) {
        anchor_node = GetNodeFromCurrentPosition();
        // Calculate the new sentence index.
        int index_in_new_node =
            combined_sentence_index - current_granularity.text.length();
        // Add the current node to the list of nodes to be returned, with a
        // text range from 0 to the start of the next sentence
        // (index_in_new_node);
        ReadAnythingAppModel::ReadAloudTextSegment segment;
        segment.id = anchor_node->id();
        segment.text_start = 0;
        segment.text_end = index_in_new_node;
        current_granularity.AddSegment(segment);
        int current_text_length = current_granularity.text.length();
        current_granularity.text +=
            anchor_node->GetTextContentUTF16().substr(0, index_in_new_node);
        current_granularity.index_map.insert(
            {{current_text_length, current_granularity.text.length()},
             segment.id});
        current_text_index_ = index_in_new_node;
        if (current_text_index_ != (int)base_text.length()) {
          // If we're in the middle of the node, there's no need to attempt
          // to find another segment, as we're at the end of the current
          // segment.
          return current_granularity;
        }
        continue;
      } else if (current_granularity.node_ids.size() > 0) {
        // If nothing has been added to the list of current nodes, we should
        // look at the next sentence within the current node. However, if
        // there have already been nodes added to the list of nodes to return
        // and we determine that the next node shouldn't be added to the
        // current sentence, we've completed the current sentence, so we can
        // return the current list.
        return current_granularity;
      }
    }

    // Add the next granularity piece within the current node.
    anchor_node = GetNodeFromCurrentPosition();
    text = anchor_node->GetTextContentUTF16();
    prev_index = current_text_index_;
    text_substr = text.substr(current_text_index_);
    // Find the next sentence within the current node.
    int new_current_text_index = GetNextSentence(text_substr) + prev_index;
    int start_index = current_text_index_;
    current_text_index_ = new_current_text_index;

    // Add the current node to the list of nodes to be returned, with a
    // text range from the starting index (the end of the previous piece of
    // the sentence) to the start of the next sentence.
    ReadAnythingAppModel::ReadAloudTextSegment segment;
    segment.id = anchor_node->id();
    segment.text_start = start_index;
    segment.text_end = new_current_text_index;
    current_granularity.AddSegment(segment);
    int current_text_length = current_granularity.text.length();
    current_granularity.text += anchor_node->GetTextContentUTF16().substr(
        start_index, current_text_index_ - start_index);
    current_granularity.index_map.insert(
        {{current_text_length, current_granularity.text.length()}, segment.id});

    // After adding the most recent granularity segment, if we're not at the
    //  end of the node, the current nodes can be returned, as we know there's
    // no further segments remaining.
    if ((size_t)current_text_index_ != text.length()) {
      return current_granularity;
    }
  }
  return current_granularity;
}

// Returns either the node or the lowest platform ancestor of the node, if it's
// a leaf.
ui::AXNode* ReadAnythingAppModel::GetNodeFromCurrentPosition() const {
  if (ax_position_->GetAnchor()->IsChildOfLeaf()) {
    return ax_position_->GetAnchor()->GetLowestPlatformAncestor();
  }

  return ax_position_->GetAnchor();
}

// Gets the next valid position from our current position within AXPosition
// AXPosition returns nodes that aren't supported by Reading Mode, so we
// need to have a bit of extra logic to ensure we're only passing along valid
// nodes.
// Some of the checks here right now are probably unneeded.
ui::AXNodePosition::AXPositionInstance
ReadAnythingAppModel::GetNextValidPositionFromCurrentPosition(
    const ReadAnythingAppModel::ReadAloudCurrentGranularity&
        current_granularity) {
  ui::AXNodePosition::AXPositionInstance new_position =
      ui::AXNodePosition::CreateNullPosition();

  ui::AXMovementOptions movement_options(
      ui::AXBoundaryBehavior::kCrossBoundary,
      ui::AXBoundaryDetection::kDontCheckInitialPosition);

  new_position = ax_position_->CreatePositionAtTextBoundary(
      ax::mojom::TextBoundary::kSentenceStart,
      ax::mojom::MoveDirection::kForward, movement_options);

  if (new_position->IsNullPosition() || new_position->AtEndOfAXTree() ||
      !new_position->GetAnchor()) {
    return new_position;
  }

  while (!IsValidAXPosition(new_position, current_granularity)) {
    ui::AXNodePosition::AXPositionInstance possible_new_position =
        new_position->CreateNextSentenceStartPosition(movement_options);
    bool use_paragraph = false;

    // If the new position and the previous position are the same, try moving
    // to the next paragraph position instead. This happens rarely, but when
    // it does, we can get stuck in an infinite loop of calling
    // CreateNextSentenceStartPosition, as it will always return the same
    // position.
    if (ArePositionsEqual(possible_new_position, new_position)) {
      use_paragraph = true;
      possible_new_position =
          new_position->CreateNextParagraphStartPosition(movement_options);

      // If after switching to use the paragraph position, the position is
      // in a null position, go ahead and return the null position so
      // speech can terminate properly. Otherwise, speech may get caught
      // in an infinite loop of searching for another item to speak when
      // there's no text left. This happens when the final node to be spoken
      // in the content is followed by an invalid character that causes
      // CreatenextSentenceStartPosition to repeatedly return the same thing.
      if (possible_new_position->IsNullPosition()) {
        return ui::AXNodePosition::AXPosition::CreateNullPosition();
      }
    }

    // If the new position is still the same as the old position after trying
    // a paragraph position, go ahead and return a null position instead, as
    // ending speech early is preferable to getting stuck in an infinite
    // loop.
    if (ArePositionsEqual(possible_new_position, new_position)) {
      return ui::AXNodePosition::AXPosition::CreateNullPosition();
    }

    if (!possible_new_position->GetAnchor()) {
      if (NodeBeenOrWillBeSpoken(current_granularity,
                                 new_position->GetAnchor()->id())) {
        // If the previous position we were looking at was previously spoken,
        // go ahead and return the null position to avoid duplicate nodes
        // being added.
        return possible_new_position;
      }
      return new_position;
    }

    new_position =
        use_paragraph
            ? new_position->CreateNextParagraphStartPosition(movement_options)
            : new_position->CreateNextSentenceStartPosition(movement_options);
  }

  return new_position;
}

int ReadAnythingAppModel::GetCurrentTextStartIndex(
    const ui::AXNodeID& node_id) {
  if (processed_granularities_on_current_page_.size() < 1) {
    return -1;
  }

  ReadAnythingAppModel::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (!current_granularity.segments.count(node_id)) {
    return -1;
  }
  ReadAnythingAppModel::ReadAloudTextSegment segment =
      current_granularity.segments[node_id];

  return segment.text_start;
}

int ReadAnythingAppModel::GetCurrentTextEndIndex(const ui::AXNodeID& node_id) {
  if (processed_granularities_on_current_page_.size() < 1) {
    return -1;
  }

  ReadAnythingAppModel::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (!current_granularity.segments.count(node_id)) {
    return -1;
  }
  ReadAnythingAppModel::ReadAloudTextSegment segment =
      current_granularity.segments[node_id];

  return segment.text_end;
}

bool ReadAnythingAppModel::NodeBeenOrWillBeSpoken(
    const ReadAnythingAppModel::ReadAloudCurrentGranularity&
        current_granularity,
    const ui::AXNodeID& id) const {
  if (base::Contains(current_granularity.segments, id)) {
    return true;
  }
  for (ReadAnythingAppModel::ReadAloudCurrentGranularity granularity :
       processed_granularities_on_current_page_) {
    if (base::Contains(granularity.segments, id)) {
      return true;
    }
  }

  return false;
}

void ReadAnythingAppModel::ResetReadAloudState() {
  ax_position_ = ui::AXNodePosition::AXPosition::CreateNullPosition();
  current_text_index_ = 0;
  processed_granularity_index_ = 0;
  processed_granularities_on_current_page_.clear();
}

bool ReadAnythingAppModel::IsTextForReadAnything(
    const ui::AXNodeID& ax_node_id) const {
  // ListMarkers will have an HTML tag of "::marker," so they won't be
  // considered text when checking for the length of the html tag. However, in
  // order to read out loud ordered bullets, nodes that have the kListMarker
  // role should be included.
  // Note: This technically will include unordered list markers like bullets,
  // but these won't be spoken because they will be filtered by the TTS engine.
  ui::AXNode* node = GetAXNode(ax_node_id);
  bool is_list_marker = node->GetRole() == ax::mojom::Role::kListMarker;

  // TODO(crbug.com/40927698): Can this be updated to IsText() instead of
  // checking the length of the html tag?
  return (GetHtmlTag(ax_node_id).length() == 0) || is_list_marker;
}

bool ReadAnythingAppModel::IsOpeningPunctuation(char& c) const {
  return (c == '(' || c == '{' || c == '[' || c == '<');
}

// We should split the current utterance at a paragraph boundary if the
// AXPosition is at the start of a paragraph and we already have nodes in
// our current granularity segment.
bool ReadAnythingAppModel::ShouldSplitAtParagraph(
    const ui::AXNodePosition::AXPositionInstance& position,
    const ReadAloudCurrentGranularity& current_granularity) const {
  return position->AtStartOfParagraph() &&
         (current_granularity.node_ids.size() > 0);
}

ui::AXNode* ReadAnythingAppModel::GetAnchorNode(
    const ui::AXNodePosition::AXPositionInstance& position) const {
  bool is_leaf = position->GetAnchor()->IsChildOfLeaf();
  // If the node is a leaf, use the parent node instead.
  return is_leaf ? position->GetAnchor()->GetLowestPlatformAncestor()
                 : position->GetAnchor();
}

bool ReadAnythingAppModel::IsValidAXPosition(
    const ui::AXNodePosition::AXPositionInstance& position,
    const ReadAnythingAppModel::ReadAloudCurrentGranularity&
        current_granularity) const {
  ui::AXNode* anchor_node = GetAnchorNode(position);
  bool was_previously_spoken =
      NodeBeenOrWillBeSpoken(current_granularity, anchor_node->id());
  bool is_text_node = IsTextForReadAnything(anchor_node->id());
  const std::set<ui::AXNodeID>* node_ids = selection_node_ids().empty()
                                               ? &display_node_ids()
                                               : &selection_node_ids();
  bool contains_node = base::Contains(*node_ids, anchor_node->id());

  return !was_previously_spoken && is_text_node && contains_node;
}

bool ReadAnythingAppModel::ArePositionsEqual(
    const ui::AXNodePosition::AXPositionInstance& position,
    const ui::AXNodePosition::AXPositionInstance& other) const {
  return position->GetAnchor() && other->GetAnchor() &&
         (position->CompareTo(*other).value_or(-1) == 0) &&
         (position->text_offset() == other->text_offset());
}

ui::AXNodeID ReadAnythingAppModel::GetNodeIdForCurrentSegmentIndex(
    int index) const {
  // If the granularity index isn't valid, return an invalid id.
  if (processed_granularity_index_ >=
      processed_granularities_on_current_page_.size()) {
    return ui::kInvalidAXNodeID;
  }

  ReadAnythingAppModel::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  std::map<std::pair<int, int>, ui::AXNodeID> index_map =
      current_granularity.index_map;
  for (const auto& [range, id] : index_map) {
    if (range.first <= index && range.second >= index) {
      // If the given index is within a range, return the associated node id.
      return id;
    }
  }

  // If the index isn't part of the current granularity's ranges, return an
  // invalid id.
  return ui::kInvalidAXNodeID;
}

int ReadAnythingAppModel::GetNextWordHighlightLength(int start_index) {
  // If the granularity index isn't valid, return 0.
  if (processed_granularity_index_ >=
          processed_granularities_on_current_page_.size() ||
      start_index < 0) {
    // 0 is returned to correspond to a 0-length or empty string.
    return 0;
  }

  ReadAnythingAppModel::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (start_index > (int)current_granularity.text.length()) {
    return 0;
  }
  // Get the remaining text in the current granularity that occurs after the
  // starting index.
  std::u16string current_text = current_granularity.text.substr(start_index);

  // Get the word length of the next word following the index.
  int word_length = GetNextWord(current_text);
  return word_length;
}

void ReadAnythingAppModel::IncrementMetric(const std::string& metric_name) {
  metric_to_count_map_[metric_name]++;
}

void ReadAnythingAppModel::LogSpeechEventCounts() {
  for (const auto& [metric, count] : metric_to_count_map_) {
    base::UmaHistogramCounts1000(metric, count);
  }
}
