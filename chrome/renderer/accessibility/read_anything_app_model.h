// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include "base/containers/contains.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_selection.h"

namespace ui {
class AXNode;
class AXSerializableTree;
}  // namespace ui

namespace ukm {
class MojoUkmRecorder;
}

// A class that holds state for the ReadAnythingAppController for the Read
// Anything WebUI app.
class ReadAnythingAppModel {
 public:
  ReadAnythingAppModel();
  ~ReadAnythingAppModel();
  ReadAnythingAppModel(const ReadAnythingAppModel& other) = delete;
  ReadAnythingAppModel& operator=(const ReadAnythingAppModel&) = delete;

  bool requires_distillation() { return requires_distillation_; }
  void set_requires_distillation(bool value) { requires_distillation_ = value; }
  bool requires_post_process_selection() {
    return requires_post_process_selection_;
  }
  void set_requires_post_process_selection(bool value) {
    requires_post_process_selection_ = value;
  }
  bool selection_from_action() { return selection_from_action_; }
  void set_selection_from_action(bool value) { selection_from_action_ = value; }

  // TODO(b/1266555): Ensure there is proper test coverage for all methods.
  // Theme
  const std::string& font_name() const { return font_name_; }
  float font_size() const { return font_size_; }
  float letter_spacing() const { return letter_spacing_; }
  float line_spacing() const { return line_spacing_; }
  int color_theme() const { return color_theme_; }
  const SkColor& foreground_color() const { return foreground_color_; }
  const SkColor& background_color() const { return background_color_; }

  // Selection.
  bool has_selection() const { return has_selection_; }
  ui::AXNodeID start_node_id() const { return start_node_id_; }
  ui::AXNodeID end_node_id() const { return end_node_id_; }
  int32_t start_offset() const { return start_offset_; }
  int32_t end_offset() const { return end_offset_; }

  bool distillation_in_progress() const { return distillation_in_progress_; }
  bool active_tree_selectable() const { return active_tree_selectable_; }
  bool is_empty() const {
    return display_node_ids_.empty() && selection_node_ids_.empty();
  }

  const ukm::SourceId& active_ukm_source_id() const {
    return active_ukm_source_id_;
  }
  const ui::AXTreeID& active_tree_id() const { return active_tree_id_; }

  const std::vector<ui::AXNodeID>& content_node_ids() const {
    return content_node_ids_;
  }
  const std::set<ui::AXNodeID>& display_node_ids() const {
    return display_node_ids_;
  }
  const std::set<ui::AXNodeID>& selection_node_ids() const {
    return selection_node_ids_;
  }

  void SetDistillationInProgress(bool distillation) {
    distillation_in_progress_ = distillation;
  }
  void SetActiveTreeSelectable(bool active_tree_selectable) {
    active_tree_selectable_ = active_tree_selectable;
  }
  void SetActiveUkmSourceId(ukm::SourceId source_id);

  void SetActiveTreeId(ui::AXTreeID tree_id) { active_tree_id_ = tree_id; }

  ui::AXNode* GetAXNode(ui::AXNodeID ax_node_id) const;
  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) const;
  bool NodeIsContentNode(ui::AXNodeID ax_node_id) const;
  void OnThemeChanged(read_anything::mojom::ReadAnythingThemePtr new_theme);
  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      const std::string& font,
      double font_size,
      read_anything::mojom::Colors color);
  void OnScroll(bool on_selection, bool from_reading_mode) const;

  void Reset(const std::vector<ui::AXNodeID>& content_node_ids);
  bool PostProcessSelection();
  // Helper functions for the rendering algorithm. Post-process the AXTree and
  // cache values before sending an `updateContent` notification to the Read
  // Anything app.ts.
  // ComputeDisplayNodeIdsForDistilledTree computes display nodes from the
  // content nodes. These display nodes will be displayed in Read Anything
  // app.ts by default.
  // ComputeSelectionNodeIds computes selection nodes from
  // the user's selection. The selection nodes list is only populated when the
  // user's selection contains nodes outside of the display nodes list. By
  // keeping two separate lists of nodes, we can switch back to displaying the
  // default distilled content without recomputing the nodes when the user
  // clears their selection or selects content inside the distilled content.
  void ComputeDisplayNodeIdsForDistilledTree();

  const std::unique_ptr<ui::AXSerializableTree>& GetTreeFromId(
      ui::AXTreeID tree_id) const;
  void AddTree(ui::AXTreeID tree_id,
               std::unique_ptr<ui::AXSerializableTree> tree);

  bool ContainsTree(ui::AXTreeID tree_id) const;

  void UnserializePendingUpdates(ui::AXTreeID tree_id);

  void ClearPendingUpdates();

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  const std::vector<ui::AXTreeUpdate>& updates,
                                  const std::vector<ui::AXEvent>& events);

  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id);

  std::map<ui::AXTreeID, std::vector<ui::AXTreeUpdate>>&
  GetPendingUpdatesForTesting();

  std::map<ui::AXTreeID, std::unique_ptr<ui::AXSerializableTree>>*
  GetTreesForTesting();

  void EraseTreeForTesting(ui::AXTreeID tree_id);

  double GetLineSpacingValue(
      read_anything::mojom::LineSpacing line_spacing) const;
  double GetLetterSpacingValue(
      read_anything::mojom::LetterSpacing letter_spacing) const;

  void IncreaseTextSize();
  void DecreaseTextSize();
  void ResetTextSize();

 private:
  void EraseTree(ui::AXTreeID tree_id);

  void InsertDisplayNode(ui::AXNodeID node);
  void ResetSelection();
  void InsertSelectionNode(ui::AXNodeID node);
  void UpdateSelection();
  void ComputeSelectionNodeIds();
  bool SelectionInsideDisplayNodes();

  void AddPendingUpdates(const ui::AXTreeID tree_id,
                         const std::vector<ui::AXTreeUpdate>& updates);

  void UnserializeUpdates(const std::vector<ui::AXTreeUpdate>& updates,
                          const ui::AXTreeID& tree_id);

  const std::vector<ui::AXTreeUpdate>& GetOrCreatePendingUpdateAt(
      ui::AXTreeID tree_id);

  void ProcessNonGeneratedEvents(const std::vector<ui::AXEvent>& events);
  void ProcessGeneratedEvents(const ui::AXEventGenerator& event_generator);

  ui::AXNode* GetParentForSelection(ui::AXNode* node);

  // State.
  // AXTrees of web contents in the browser’s tab strip.
  std::map<ui::AXTreeID, std::unique_ptr<ui::AXSerializableTree>> trees_;

  // The AXTreeID of the currently active web contents.
  ui::AXTreeID active_tree_id_ = ui::AXTreeIDUnknown();

  // The UKM source ID of the main frame of the active web contents, whose
  // AXTree has ID active_tree_id_. This is used for metrics collection.
  ukm::SourceId active_ukm_source_id_ = ukm::kInvalidSourceId;

  // Certain websites (e.g. Docs and PDFs) are not distillable with selection.
  bool active_tree_selectable_ = true;

  // Distillation is slow and happens out-of-process when Screen2x is running.
  // This boolean marks when distillation is in progress to avoid sending
  // new distillation requests during that time.
  bool distillation_in_progress_ = false;

  // A mapping of a tree ID to a queue of pending updates on the active AXTree,
  // which will be unserialized once distillation completes.
  std::map<ui::AXTreeID, std::vector<ui::AXTreeUpdate>> pending_updates_map_;

  // The node IDs identified as main by the distiller. These are static text
  // nodes when generated by Screen2x. When generated by the rules-based
  // distiller, these are heading or paragraph subtrees.
  std::vector<ui::AXNodeID> content_node_ids_;

  // This contains all ancestors and descendants of each content node. These
  // nodes will be displayed in the Read Anything app if there is no user
  // selection or if the users selection is contained within these nodes.
  std::set<ui::AXNodeID> display_node_ids_;

  // If the user's selection contains nodes outside of display_node_ids, this
  // contains all nodes between the start and end nodes of the selection.
  std::set<ui::AXNodeID> selection_node_ids_;

  // Theme information.
  std::string font_name_ = string_constants::kReadAnythingPlaceholderFontName;
  float font_size_ = kReadAnythingDefaultFontScale;
  float letter_spacing_ =
      (int)read_anything::mojom::LetterSpacing::kDefaultValue;
  float line_spacing_ = (int)read_anything::mojom::LineSpacing::kDefaultValue;
  SkColor background_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  SkColor foreground_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  int color_theme_ = (int)read_anything::mojom::Colors::kDefaultValue;

  // Selection information.
  bool has_selection_ = false;
  ui::AXNodeID start_node_id_ = ui::kInvalidAXNodeID;
  ui::AXNodeID end_node_id_ = ui::kInvalidAXNodeID;
  int32_t start_offset_ = -1;
  int32_t end_offset_ = -1;
  bool requires_distillation_ = false;
  bool requires_post_process_selection_ = false;
  bool selection_from_action_ = false;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // Used to keep track of how many selections were made for the
  // active_ukm_source_id_. Only recorded during the select-to-distill flow
  // (when the empty state page is shown).
  int32_t num_selections_ = 0;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
