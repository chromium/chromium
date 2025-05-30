// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "chrome/renderer/accessibility/read_anything/read_aloud_traversal_utils.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_node_utils.h"
#include "content/public/renderer/render_thread.h"
#include "services/strings/grit/services_strings.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/accessibility/ax_tree_update_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// TODO(crbug.com/355925253): Consider removing one constant when a working
// combination is found.
base::TimeDelta kTimeElapsedSincePageLoadForDataCollection = base::Seconds(30);
base::TimeDelta kTimeElapsedSinceTreeChangedForDataCollection =
    base::Seconds(30);

const ui::AXNode* GetUnignoredParentForSelection(const ui::AXNode* node) {
  const ui::AXNode* parent = node;
  while (const ui::AXNode* ancestor =
             parent->GetUnignoredParentCrossingTreeBoundary()) {
    static constexpr auto should_skip = [](const ui::AXNode* node) {
      // When a link is highlighted, the start node has an "inline" display; the
      // common parent of all siblings is the first ancestor which has a "block"
      // display. Also skip over "list-item" so all items in a list are
      // displayed as siblings, to avoid misnumbering.
      const std::string_view display =
          node->GetStringAttribute(ax::mojom::StringAttribute::kDisplay);
      return base::Contains(display, "inline") ||
             base::Contains(display, "list-item");
    };
    if (!should_skip(ancestor)) {
      return ancestor;
    }
    parent = ancestor;
  }
  return parent == node ? nullptr : parent;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ReadAnythingHeuristics)
enum class ReadAnythingHeuristics {
  kNone = 0,
  kNodeNotFound = 1,
  kInvisibleOrIgnored = 2,
  kNotExpanded = 3,
  kNoDeepsetLastDecendent = 4,
  kMaxValue = kNoDeepsetLastDecendent
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingHeuristics)

void RecordHeuristicMetric(ReadAnythingHeuristics heuristic) {
  base::UmaHistogramEnumeration("Accessibility.ReadAnything.Heuristics",
                                heuristic);
}

}  // namespace

ReadAnythingAppModel::AXTreeInfo::AXTreeInfo(
    std::unique_ptr<ui::AXTreeManager> manager)
    : manager(std::move(manager)) {}

ReadAnythingAppModel::AXTreeInfo::~AXTreeInfo() = default;

ReadAnythingAppModel::SelectionEndpoint::SelectionEndpoint(
    const ui::AXSelection& selection,
    Source source)
    : id(source == Source::kAnchor ? selection.anchor_object_id
                                   : selection.focus_object_id),
      offset(source == Source::kAnchor ? selection.anchor_offset
                                       : selection.focus_offset) {}

ReadAnythingAppModel::ReadAnythingAppModel() {
  ResetTextSize();
}

ReadAnythingAppModel::~ReadAnythingAppModel() = default;

void ReadAnythingAppModel::InsertIdIfNotIgnored(
    ui::AXNodeID id,
    std::set<ui::AXNodeID>& non_ignored_ids) {
  // If the node is not in the active tree (this could happen when RM is still
  // loading), ignore it.
  const ui::AXNode* const ax_node = GetAXNode(id);
  if (!ax_node) {
    return;
  }

  if (!a11y::IsIgnored(ax_node, is_pdf_)) {
    non_ignored_ids.insert(id);
  }
}

void ReadAnythingAppModel::OnSettingsRestoredFromPrefs(
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing,
    std::string font_name,
    double font_size,
    bool links_enabled,
    bool images_enabled,
    read_anything::mojom::Colors color) {
  line_spacing_ = line_spacing;
  letter_spacing_ = letter_spacing;
  font_name_ = std::move(font_name);
  SetFontSize(font_size);
  links_enabled_ = links_enabled;
  images_enabled_ = images_enabled;
  color_theme_ = color;
}

void ReadAnythingAppModel::Reset(std::vector<ui::AXNodeID> content_node_ids) {
  content_node_ids_ = std::move(content_node_ids);
  display_node_ids_.clear();
  distillation_in_progress_ = false;
  requires_post_process_selection_ = false;
  selections_from_reading_mode_ = 0;
  ResetSelection();
}

void ReadAnythingAppModel::ResetSelection() {
  selection_node_ids_.clear();
  start_ = SelectionEndpoint();
  end_ = SelectionEndpoint();
}

bool ReadAnythingAppModel::PostProcessSelection() {
  CHECK_NE(active_tree_id_, ui::AXTreeIDUnknown());
  const auto it = tree_infos_.find(active_tree_id_);
  CHECK(it != tree_infos_.end());

  requires_post_process_selection_ = false;

  // If the new selection came from the side panel, we don't need to draw
  // anything in the side panel, since whatever was being selected had to have
  // been drawn already.
  // If there is no previous selection, we don't need to check whether it was
  // inside the distilled content. In this case, we will only draw if the new
  // selection is outside the distilled content.
  // If there was a previous selection outside the distilled content, we always
  // redraw. This will be either a) the new selected content or b) the original
  // distilled content if the new selection is inside that or was cleared.
  const auto selection_in_distilled_content = [&] {
    return display_node_ids_.contains(start_.id) &&
           display_node_ids_.contains(end_.id);
  };
  const bool need_to_draw = (selections_from_reading_mode_ == 0) &&
                            has_selection() &&
                            !selection_in_distilled_content();
  const bool was_empty = is_empty();

  // Update selection.
  ResetSelection();
  if (const ui::AXSelection selection =
          GetTreeFromId(active_tree_id_)->GetUnignoredSelection();
      selection.anchor_object_id != ui::kInvalidAXNodeID &&
      selection.focus_object_id != ui::kInvalidAXNodeID &&
      !selection.IsCollapsed()) {
    // Identify the start and end node ids and offsets. The start node comes
    // earlier than end node in the tree order. We need to send the selection to
    // JS in forward order. If they are sent as backward selections, JS will
    // collapse the selection so no selection will be rendered in Read Anything.
    auto source_start = SelectionEndpoint::Source::kAnchor,
         source_end = SelectionEndpoint::Source::kFocus;
    if (selection.is_backward) {
      std::swap(source_start, source_end);
    }
    start_ = SelectionEndpoint(selection, source_start);
    end_ = SelectionEndpoint(selection, source_end);
  }

  if (!has_selection()) {
    return need_to_draw;
  }

  if (was_empty) {
    base::UmaHistogramEnumeration(kEmptyStateHistogramName,
                                  EmptyState::kShownWithSelectionAfter);
    ++it->second->num_selections;
  }

  if (selection_in_distilled_content()) {
    return need_to_draw;
  }

  const ui::AXNode* node = GetAXNode(start_.id);
  const ui::AXNode* end = GetAXNode(end_.id);
  DUMP_WILL_BE_CHECK(node && end);
  if (!node || !end) {
    // Fail gracefully if the returned nodes are ever missing.
    // This should never happen given that the AXSelection object is retrieved
    // from the active tree.
    return false;
  }

  // The main panel selection contains content outside of the distilled
  // content. Find the selected nodes to display instead of the distilled
  // content.
  if (!node->IsInvisibleOrIgnored() && !end->IsInvisibleOrIgnored()) {
    // Add all ancestor ids of start node, including the start node itself.
    for (base::queue<ui::AXNode*> ancestors =
             node->GetAncestorsCrossingTreeBoundaryAsQueue();
         !ancestors.empty(); ancestors.pop()) {
      InsertIdIfNotIgnored(ancestors.front()->id(), selection_node_ids_);
    }

    // Find the parent of the start and end nodes so we can look at nearby
    // sibling nodes. Since the start and end nodes might be in different
    // section of the tree, get the parents for start and end separately.
    // Otherwise, the end selection might not render.
    node = GetUnignoredParentForSelection(node);
    end = GetUnignoredParentForSelection(end);
    if (end) {
      end = end->GetDeepestLastUnignoredDescendantCrossingTreeBoundary();
      if (node && end) {
        // Traverse the tree from the first sibling node to the last sibling
        // node, inclusive. This ensures that when select-to-distill is used to
        // distill non-distillable content (such as Gmail), text outside of the
        // selected portion but on the same line is still distilled, even if
        // there's special formatting.
        // TODO(crbug.com/40802192): Consider using ax_position.h here to better
        // manage selection.
        for (node = node->GetFirstUnignoredChildCrossingTreeBoundary();
             node && node->CompareTo(*end).value_or(1) <= 0;
             node = node->GetNextUnignoredInTreeOrder()) {
          InsertIdIfNotIgnored(node->id(), selection_node_ids_);
        }
      }
    }
  }
  return true;
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
  if (ContentNodesOnlyContainHeadings()) {
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
    if (!content_node) {
      RecordHeuristicMetric(ReadAnythingHeuristics::kNodeNotFound);
      continue;
    }

    if (content_node->IsInvisibleOrIgnored()) {
      RecordHeuristicMetric(ReadAnythingHeuristics::kInvisibleOrIgnored);
      continue;
    }

    // Ignore aria-expanded for editables.
    if (content_node->data().SupportsExpandCollapse() &&
        !content_node->HasState(ax::mojom::State::kRichlyEditable)) {
      // Capture the expanded state. ARIA expanded is not supported by all
      // element types, but gmail for example uses it anyways. Check the
      // attribute directly for that reason.
      if (!content_node->HasState(ax::mojom::State::kExpanded)) {
        // Don't include collapsed aria-expanded items.
        RecordHeuristicMetric(ReadAnythingHeuristics::kNotExpanded);
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
      InsertIdIfNotIgnored(ancestor_id, display_node_ids_);
    }

    // Add all descendant ids to the set.
    ui::AXNode* next_node = content_node;
    ui::AXNode* deepest_last_descendant =
        content_node->GetDeepestLastUnignoredDescendant();
    if (!deepest_last_descendant) {
      RecordHeuristicMetric(ReadAnythingHeuristics::kNoDeepsetLastDecendent);
      continue;
    }
    while (next_node != deepest_last_descendant) {
      next_node = next_node->GetNextUnignoredInTreeOrder();
      InsertIdIfNotIgnored(next_node->id(), display_node_ids_);
    }

    RecordHeuristicMetric(ReadAnythingHeuristics::kNone);
  }
}

ui::AXSerializableTree* ReadAnythingAppModel::GetActiveTree() const {
  return GetTreeFromId(active_tree_id_);
}

ui::AXSerializableTree* ReadAnythingAppModel::GetTreeFromId(
    const ui::AXTreeID& tree_id) const {
  // If the tree id is unknown or not associated with a tree, fail gracefully,
  // since reading mode can sometimes get into a state with invalid data, and
  // failing gracefully is preferable to crashing. Use DUMP_WILL_BE_CHECK to
  // collect information on this bad state without actually crashing.
  DUMP_WILL_BE_CHECK(ContainsTree(tree_id));
  DUMP_WILL_BE_CHECK(tree_id != ui::AXTreeIDUnknown());
  if (!ContainsTree(tree_id) || tree_id == ui::AXTreeIDUnknown()) {
    return nullptr;
  }
  return static_cast<ui::AXSerializableTree*>(
      tree_infos_.at(tree_id)->manager->ax_tree());
}

bool ReadAnythingAppModel::ContainsTree(const ui::AXTreeID& tree_id) const {
  return base::Contains(tree_infos_, tree_id);
}

bool ReadAnythingAppModel::ContainsActiveTree() const {
  return ContainsTree(active_tree_id_);
}

void ReadAnythingAppModel::SetUrlInformationCallback(
    base::OnceCallback<void()> callback) {
  // If the given tree already has its url information set, run the callback
  // immediately.
  if (tree_infos_.contains(active_tree_id_) &&
      tree_infos_.at(active_tree_id_)->is_url_information_set) {
    std::move(callback).Run();
    return;
  }

  set_url_information_callback_ = std::move(callback);
}

void ReadAnythingAppModel::SetTreeInfoUrlInformation(
    ReadAnythingAppModel::AXTreeInfo& tree_info) {
  // If the url information has already been set for this tree, do nothing.
  if (tree_info.is_url_information_set) {
    return;
  }

  // If the tree manager is not the root manager, do nothing.
  CHECK(tree_info.manager);
  if (!tree_info.manager->IsRoot()) {
    return;
  }

  // If the tree doesn't have a root, or the root doesn't have a url set, do
  // nothing.
  const ui::AXNode* const root = tree_info.manager->GetRoot();
  if (!root || !root->HasStringAttribute(ax::mojom::StringAttribute::kUrl)) {
    return;
  }

  // A Google Docs URL is in the form of "https://docs.google.com/document*" or
  // "https://docs.sandbox.google.com/document*".
  const GURL url(root->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  tree_info.is_reload =
      !previous_tree_url_.empty() && (previous_tree_url_ == url.GetContent());

  tree_info.is_docs = url.SchemeIsHTTPOrHTTPS() &&
                      (url.DomainIs("docs.google.com") ||
                       url.DomainIs("docs.sandbox.google.com")) &&
                      url.path().starts_with("/document") &&
                      !url.ExtractFileName().empty();

  tree_info.is_url_information_set = true;
  previous_tree_url_ = url.GetContent();

  if (!set_url_information_callback_.is_null()) {
    std::move(set_url_information_callback_).Run();
  }
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

bool ReadAnythingAppModel::IsReload() const {
  if (!tree_infos_.contains(active_tree_id_)) {
    return false;
  }

  return tree_infos_.at(active_tree_id_)->is_reload;
}

void ReadAnythingAppModel::AddPendingUpdates(const ui::AXTreeID& tree_id,
                                             Updates& updates) {
  pending_updates_[tree_id].emplace_back(std::move(updates));
}

void ReadAnythingAppModel::ClearPendingUpdates() {
  pending_updates_.clear();
}

void ReadAnythingAppModel::UnserializePendingUpdates(
    const ui::AXTreeID& tree_id) {
  if (!pending_updates_.contains(tree_id)) {
    return;
  }
  // TODO(crbug.com/40802192): Ensure there are no crashes/unexpected behavior
  // if an accessibility event is received on the same tree after
  // unserialization has begun.
  std::vector<Updates> updates = pending_updates_.extract(tree_id).mapped();
  for (Updates update : updates) {
    // Unserialize the updates in batches in the groupings in which they were
    // received by AccessibilityEventReceived.
    DCHECK(update.empty() || tree_id == active_tree_id_);
    UnserializeUpdates(update, tree_id);
  }
}

void ReadAnythingAppModel::UnserializeUpdates(Updates& updates,
                                              const ui::AXTreeID& tree_id) {
  if (updates.empty()) {
    return;
  }

  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  const auto it = tree_infos_.find(tree_id);
  DCHECK(it != tree_infos_.end());
  auto* const tree =
      static_cast<ui::AXSerializableTree*>(it->second->manager->ax_tree());
  CHECK(tree);

  // Build an event generator prior to any unserializations.
  ui::AXEventGenerator event_generator(tree);

  // Unserialize the updates.
  const size_t prev_tree_size = tree->size();
  for (const ui::AXTreeUpdate& update : updates) {
    // If a tree update without a valid root is received for a tree without
    // a valid root, it is likely the tree was previously destroyed and reading
    // mode received a delayed accessibility event. This can happen on pages
    // with frequent updates. If this happens, don't continue trying to
    // unserialize because the data is bad.
    DUMP_WILL_BE_CHECK(tree->root() || update.root_id != ui::kInvalidAXNodeID);
    if (!tree->root() && update.root_id == ui::kInvalidAXNodeID) {
      return;
    }
    tree->Unserialize(update);
  }

  // Set URL info if it hasn't already been set.
  SetTreeInfoUrlInformation(*it->second);

  ProcessGeneratedEvents(event_generator, prev_tree_size, tree->size());
}

void ReadAnythingAppModel::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    Updates& updates,
    std::vector<ui::AXEvent>& events,
    bool speech_playing) {
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  // Create a new tree if an event is received for a tree that is not yet in
  // the tree list.
  if (!ContainsTree(tree_id)) {
    auto new_tree = std::make_unique<ui::AXSerializableTree>();
    for (auto& observer : observers_) {
      observer.OnTreeAdded(new_tree.get());
    }
    tree_infos_.emplace(
        tree_id, std::make_unique<AXTreeInfo>(
                     std::make_unique<ui::AXTreeManager>(std::move(new_tree))));
    // If we previously received UKM source info for this tree_id, set the
    // UKM source now that the tree information has been added to tree_infos_.
    if (tree_id == active_tree_id_ && pending_ukm_sources_.count(tree_id) > 0) {
      ukm::SourceId ukm_source_id = pending_ukm_sources_[tree_id];
      pending_ukm_sources_.erase(tree_id);
      SetUkmSourceId(ukm_source_id);
    }
  }

  if (may_use_child_for_active_tree_) {
    // If this is the original root tree id, set it back to the active tree
    // in case there has been a delay in receiving valid accessibility tree
    // updates.
    if (root_tree_id_ == tree_id) {
      SetRootTreeId(root_tree_id_);
    } else if (active_tree_id_ != ui::AXTreeIDUnknown() &&
               active_tree_id_ != tree_id &&
               child_tree_ids_.find(tree_id) != child_tree_ids_.end()) {
      // If read aloud is searching for a child tree to distill and this tree id
      // matches one of the possible child ids, set the active tree to this tree
      // so that it can be distilled.
      SetActiveTreeId(tree_id);
    }
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
    }
    // We need to unserialize old updates before we can unserialize the new
    // ones.
    UnserializePendingUpdates(tree_id);
    UnserializeUpdates(updates, tree_id);
    ProcessNonGeneratedEvents(events);
  } else {
    UnserializeUpdates(updates, tree_id);
  }

  if (features::IsDataCollectionModeForScreen2xEnabled() && updates.size()) {
    waiting_for_tree_change_timer_trigger_ = true;
    timer_since_tree_changed_for_data_collection_.Start(
        FROM_HERE, kTimeElapsedSinceTreeChangedForDataCollection,
        base::BindRepeating(&ReadAnythingAppModel::OnTreeChangeTimerTriggered,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ReadAnythingAppModel::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  // `OnAXTreeDestroyed()` is called whenever the `AXActionHandler` in the
  // browser learns that an `AXTree` was destroyed. This could be from any tab,
  // not just the active one; therefore many `tree_id`s will not be found in
  // `tree_infos_`.
  const auto it = tree_infos_.find(tree_id);
  if (it == tree_infos_.end()) {
    return;
  }

  if (active_tree_id_ == tree_id) {
    // TODO(crbug.com/40802192): If distillation is in progress, cancel the
    // distillation request.
    active_tree_id_ = ui::AXTreeIDUnknown();
  }

  for (ui::AXTree* const ax_tree = it->second->manager->ax_tree();
       auto& observer : observers_) {
    observer.OnTreeRemoved(ax_tree);
  }

  tree_infos_.erase(it);

  // Any pending updates associated with the erased tree should also be dropped.
  pending_updates_.erase(tree_id);
}

ukm::SourceId ReadAnythingAppModel::GetUkmSourceId() const {
  if (base::Contains(tree_infos_, active_tree_id_)) {
    ReadAnythingAppModel::AXTreeInfo* tree_info =
        tree_infos_.at(active_tree_id_).get();
    if (tree_info) {
      return tree_info->ukm_source_id;
    }
  }
  return ukm::kInvalidSourceId;
}

void ReadAnythingAppModel::SetUkmSourceIdForTree(const ui::AXTreeID& tree,
                                                 ukm::SourceId ukm_source_id) {
  // We may receive an OnActiveAXTreeIDChanged event on a tree before we've
  // received an AccessibilityEventReceived event adding the tree to
  // tree_infos_. When this happens, we should keep track of the ukm_source_id,
  // and later, if the tree is added to tree_infos_ while it's still active,
  // we can try again to set the ukm source.
  if (!base::Contains(tree_infos_, active_tree_id_)) {
    pending_ukm_sources_[tree] = ukm_source_id;
    return;
  }

  SetUkmSourceId(ukm_source_id);
}

void ReadAnythingAppModel::SetUkmSourceId(ukm::SourceId ukm_source_id) {
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

int ReadAnythingAppModel::GetNumSelections() const {
  if (base::Contains(tree_infos_, active_tree_id_)) {
    ReadAnythingAppModel::AXTreeInfo* tree_info =
        tree_infos_.at(active_tree_id_).get();
    if (tree_info) {
      return tree_info->num_selections;
    }
  }
  return 0;
}

void ReadAnythingAppModel::SetNumSelections(int num_selections) {
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
  if (!tree) {
    return nullptr;
  }
  return tree->GetFromId(ax_node_id);
}

bool ReadAnythingAppModel::NodeIsContentNode(ui::AXNodeID ax_node_id) const {
  return base::Contains(content_node_ids_, ax_node_id);
}

void ReadAnythingAppModel::AdjustTextSize(int increment) {
  SetFontSize(font_size_, increment);
}

void ReadAnythingAppModel::ResetTextSize() {
  SetFontSize(2.0f);
}

void ReadAnythingAppModel::OnScroll(bool on_selection,
                                    bool from_reading_mode) const {
  // Enum for logging how a scroll occurs.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ReadAnythingScrollEvent)
  enum class ReadAnythingScrollEvent {
    kSelectedSidePanel = 0,
    kSelectedMainPanel = 1,
    kScrolledSidePanel = 2,
    kScrolledMainPanel = 3,
    kMaxValue = kScrolledMainPanel,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingScrollEvent)
  using enum ReadAnythingScrollEvent;

  ReadAnythingScrollEvent event;
  if (on_selection) {
    // If the scroll event came from the side panel because of a selection, then
    // this means the main panel was selected, causing the side panel to scroll
    // & vice versa.
    event = from_reading_mode ? kSelectedMainPanel : kSelectedSidePanel;
  } else {
    event = from_reading_mode ? kScrolledSidePanel : kScrolledMainPanel;
  }
  base::UmaHistogramEnumeration("Accessibility.ReadAnything.ScrollEvent",
                                event);
}

void ReadAnythingAppModel::SetRootTreeId(ui::AXTreeID root_tree_id) {
  root_tree_id_ = root_tree_id;
  SetActiveTreeId(root_tree_id);

  // Whenever reading mode receives a signal of a new active tree id, clear
  // previous attempts to search for a valid child tree on the active tree in
  // case the new active tree is distillable.
  may_use_child_for_active_tree_ = false;
  child_tree_ids_.clear();
}

void ReadAnythingAppModel::SetActiveTreeId(ui::AXTreeID active_tree_id) {
  // Unserialize any updates on the previous active tree;
  // Otherwise, this can cause tree inconsistency issues if reading mode later
  // incorrectly receives updates from the old tree.
  if (active_tree_id_ != active_tree_id &&
      active_tree_id_ != ui::AXTreeIDUnknown() && ContainsActiveTree()) {
    UnserializePendingUpdates(active_tree_id_);
  }

  active_tree_id_ = std::move(active_tree_id);
  // If data collection mode for screen2x is enabled, begin
  // `timer_since_page_load_for_data_collection_` from here. This is a
  // one-shot timer which times 30 seconds from when the active AXTree changes.
  // This is one of two timers associated with the data collection flow. When
  // either of these timers expires, this triggers the screen2x distillation
  // data collection flow.
  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    timer_since_page_load_for_data_collection_.Start(
        FROM_HERE, kTimeElapsedSincePageLoadForDataCollection,
        base::BindOnce(&ReadAnythingAppModel::OnPageLoadTimerTriggered,
                       weak_ptr_factory_.GetWeakPtr()));

    // If tree does not change until the page load timer triggers, assume that
    // the page is not changing. `waiting_for_tree_change_timer_trigger_` is set
    // again when tree changes.
    if (timer_since_tree_changed_for_data_collection_.IsRunning()) {
      timer_since_tree_changed_for_data_collection_.Stop();
    }
    waiting_for_tree_change_timer_trigger_ = false;
  }
}

void ReadAnythingAppModel::ProcessNonGeneratedEvents(
    const std::vector<ui::AXEvent>& events) {
  // Marks if an event has happened that can affect collection of training data
  // for Screen2x.
  bool delay_screen2x_training_data_collection_ = false;

  // Note that this list of events may overlap with generated events in the
  // model. It's up to the consumer to pick but its generally good to prefer
  // generated. The consumer should not process the same event here and for
  // generated events.
  for (auto& event : events) {
    switch (event.event_type) {
      case ax::mojom::Event::kLoadComplete:
        requires_distillation_ = true;
        page_finished_loading_ = true;
        delay_screen2x_training_data_collection_ = true;
        // TODO(accessibility): Some pages may never completely load; use a
        // timer with a reasonable delay to force distillation -> drawing.
        // Investigate if this is needed.
        break;

      case ax::mojom::Event::kLocationChanged:
        delay_screen2x_training_data_collection_ = true;
        break;

      case ax::mojom::Event::kBlur:
        // Closing ads sometimes sends this event but we also get this when
        // keyboard focus changes. Only try to redistill if we have no content
        // right now.
        if (features::IsReadAnythingReadAloudEnabled() &&
            content_node_ids_.size() == 0) {
          requires_distillation_ = true;
        }
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
        // After the user finishes typing something we wait for a timer and
        // redraw to capture the input.
        if (event.event_from == ax::mojom::EventFrom::kUser &&
            event.event_intents.size() > 0) {
          reset_draw_timer_ = true;
        }
        break;
      case ax::mojom::Event::kAriaAttributeChangedDeprecated:
      case ax::mojom::Event::kMenuListValueChangedDeprecated:
        NOTREACHED();
    }
  }

  // If data collection mode for screen2x is enabled, begin
  // `timer_since_tree_changed_for_data_collection_` from here. This is a
  // repeating one-shot timer which times 10 seconds from page load and
  // resets every time the accessibility tree changes in a way that affects data
  // collection. This is one of two timers associated with the data collection
  // flow. When both of these timers expire, the screen2x distillation data
  // collection flow is triggered.
  if (features::IsDataCollectionModeForScreen2xEnabled() &&
      delay_screen2x_training_data_collection_) {
    waiting_for_tree_change_timer_trigger_ = true;
    timer_since_tree_changed_for_data_collection_.Start(
        FROM_HERE, kTimeElapsedSinceTreeChangedForDataCollection,
        base::BindRepeating(&ReadAnythingAppModel::OnTreeChangeTimerTriggered,
                            weak_ptr_factory_.GetWeakPtr()));
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
        requires_post_process_selection_ = true;
        break;
      case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
        if (!features::IsReadAnythingReadAloudEnabled() ||
            event.event_params->event_from == ax::mojom::EventFrom::kUser) {
          requires_distillation_ = true;
        }
        break;
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
      case ui::AXEventGenerator::Event::COLLAPSED:
        if (features::IsReadAnythingReadAloudEnabled()) {
          ResetSelection();
          requires_post_process_selection_ = false;
          redraw_required_ = true;
        }
        break;
      case ui::AXEventGenerator::Event::EXPANDED:
        if (features::IsReadAnythingReadAloudEnabled()) {
          if (base::Contains(content_node_ids_, event.node_id)) {
            redraw_required_ = true;
          } else {
            requires_distillation_ = true;
          }
        }
        break;
      // Audit these events e.g. to trigger distillation.
      case ui::AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
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
      case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
      case ui::AXEventGenerator::Event::DETAILS_CHANGED:
      case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
      case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
      case ui::AXEventGenerator::Event::ENABLED_CHANGED:
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
        if (!features::IsReadAnythingReadAloudEnabled() &&
            last_expanded_node_id_ == event.node_id) {
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

bool ReadAnythingAppModel::ScreenAIServiceReadyForDataCollection() const {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  return screen_ai_service_ready_for_data_collection_;
}

void ReadAnythingAppModel::SetScreenAIServiceReadyForDataCollection() {
  screen_ai_service_ready_for_data_collection_ = true;
  MaybeRunDataCollectionForScreen2xCallback();
}

bool ReadAnythingAppModel::PageFinishedLoadingForDataCollection() const {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  return !waiting_for_page_load_completion_timer_trigger_ &&
         !waiting_for_tree_change_timer_trigger_;
}

void ReadAnythingAppModel::OnPageLoadTimerTriggered() {
  CHECK(waiting_for_page_load_completion_timer_trigger_);
  waiting_for_page_load_completion_timer_trigger_ = false;
  MaybeRunDataCollectionForScreen2xCallback();
}

void ReadAnythingAppModel::OnTreeChangeTimerTriggered() {
  CHECK(waiting_for_tree_change_timer_trigger_);
  waiting_for_tree_change_timer_trigger_ = false;
  MaybeRunDataCollectionForScreen2xCallback();
}

void ReadAnythingAppModel::SetDataCollectionForScreen2xCallback(
    base::OnceCallback<void()> callback) {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  data_collection_for_screen2x_callback_ = std::move(callback);
}

void ReadAnythingAppModel::MaybeRunDataCollectionForScreen2xCallback() {
  CHECK(features::IsDataCollectionModeForScreen2xEnabled());
  if (!PageFinishedLoadingForDataCollection() ||
      !ScreenAIServiceReadyForDataCollection()) {
    return;
  }
  if (data_collection_for_screen2x_callback_.is_null()) {
    LOG(ERROR) << "Callback not set or triggered more than once.";
    return;
  }
  std::move(data_collection_for_screen2x_callback_).Run();
}

void ReadAnythingAppModel::SetBaseLanguageCode(std::string base_language_code) {
  DCHECK(!base_language_code.empty());
  base_language_code_ = std::move(base_language_code);
  supported_fonts_ = GetSupportedFonts(base_language_code_);
}

void ReadAnythingAppModel::AddObserver(ModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingAppModel::RemoveObserver(ModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingAppModel::SetFontSize(double font_size, int increment) {
  font_size_ = AdjustFontScale(font_size, increment);
}

const std::set<ui::AXNodeID>* ReadAnythingAppModel::GetCurrentlyVisibleNodes()
    const {
  return selection_node_ids_.empty() ? &display_node_ids()
                                     : &selection_node_ids_;
}

void ReadAnythingAppModel::AllowChildTreeForActiveTree(bool use_child_tree) {
  may_use_child_for_active_tree_ = use_child_tree;

  if (!may_use_child_for_active_tree_) {
    child_tree_ids_.clear();
  }

  ui::AXSerializableTree* active_tree = GetTreeFromId(active_tree_id_);
  if (!active_tree) {
    return;
  }
  std::set<ui::AXTreeID> child_ids = active_tree->GetAllChildTreeIds();
  if (!child_ids.size()) {
    return;
  }

  // Store all the possible child tree ids that could be used as the active
  // tree if they have distillable content.
  child_tree_ids_.insert(child_ids.begin(), child_ids.end());
}
