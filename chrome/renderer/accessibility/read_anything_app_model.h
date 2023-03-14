// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_selection.h"

namespace ui {
class AXNode;
class AXSerializableTree;
class AXTreeObserver;
}  // namespace ui

class ReadAnythingAppControllerTest;
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

  bool distillation_in_progress() const { return distillation_in_progress_; }
  const ukm::SourceId& active_ukm_source_id() const {
    return active_ukm_source_id_;
  }
  const ui::AXTreeID& active_tree_id() const { return active_tree_id_; }

  const std::vector<ui::AXTreeUpdate>& pending_updates() const {
    return pending_updates_;
  }

#if DCHECK_IS_ON()
  const ui::AXTreeID& pending_updates_bundle_id() const {
    return pending_updates_bundle_id_;
  }
  void SetPendingUpdatesBundleId(ui::AXTreeID id) {
    pending_updates_bundle_id_ = id;
  }
#endif

  void SetDistillationInProgress(bool distillation) {
    distillation_in_progress_ = distillation;
  }
  void SetActiveUkmSourceId(ukm::SourceId source_id) {
    active_ukm_source_id_ = source_id;
  }
  void SetActiveTreeId(ui::AXTreeID tree_id) { active_tree_id_ = tree_id; }

  void SetStart(ui::AXNodeID start_node_id, int32_t start_offset);
  void SetEnd(ui::AXNodeID end_node_id, int32_t end_offset);

  const std::vector<ui::AXNodeID>& content_node_ids() const {
    return content_node_ids_;
  }
  const std::set<ui::AXNodeID>& display_node_ids() const {
    return display_node_ids_;
  }

  void OnThemeChanged(read_anything::mojom::ReadAnythingThemePtr new_theme);

  void ResetSelection();
  void InsertDisplayNode(ui::AXNodeID node);
  void Reset(const std::vector<ui::AXNodeID>& content_node_ids);

  const std::unique_ptr<ui::AXSerializableTree>& GetTreeFromId(
      ui::AXTreeID tree_id) const;
  void AddTree(ui::AXTreeID tree_id,
               std::unique_ptr<ui::AXSerializableTree> tree);

  bool ContainsTree(ui::AXTreeID tree_id) const;

  void EraseTree(ui::AXTreeID tree_id);

  void UnserializePendingUpdates();

  void ClearPendingUpdates();

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  const std::vector<ui::AXTreeUpdate>& updates,
                                  ui::AXTreeObserver* tree_observer);

 private:
  friend ReadAnythingAppControllerTest;
  double GetLetterSpacingValue(
      read_anything::mojom::LetterSpacing letter_spacing) const;
  double GetLineSpacingValue(
      read_anything::mojom::LineSpacing line_spacing) const;
  void AddPendingUpdates(const std::vector<ui::AXTreeUpdate>& updates);

  void UnserializeUpdates(std::vector<ui::AXTreeUpdate> updates,
                          const ui::AXTreeID& tree_id);

  size_t NumTreesForTesting() const;

  // State.
  // AXTrees of web contents in the browser’s tab strip.
  std::map<ui::AXTreeID, std::unique_ptr<ui::AXSerializableTree>> trees_;

  // The AXTreeID of the currently active web contents.
  ui::AXTreeID active_tree_id_ = ui::AXTreeIDUnknown();

  // The UKM source ID of the main frame of the active web contents, whose
  // AXTree has ID active_tree_id_. This is used for metrics collection.
  ukm::SourceId active_ukm_source_id_ = ukm::kInvalidSourceId;

  // Distillation is slow and happens out-of-process when Screen2x is running.
  // This boolean marks when distillation is in progress to avoid sending
  // new distillation requests during that time.
  bool distillation_in_progress_ = false;

  // A queue of pending updates on the active AXTree, which will be
  // unserialized once distillation completes.
  std::vector<ui::AXTreeUpdate> pending_updates_;

#if DCHECK_IS_ON()
  // The bundle ID for the pending updates.
  ui::AXTreeID pending_updates_bundle_id_ = ui::AXTreeIDUnknown();
#endif

  // The node IDs identified as main by the distiller. These are static text
  // nodes when generated by Screen2x. When generated by the rules-based
  // distiller, these are heading or paragraph subtrees.
  std::vector<ui::AXNodeID> content_node_ids_;

  // The node IDs that are displayed in the Read Anything app. This contains
  // all ancestors and descendants of each content node. Or, if no content
  // nodes were identified, this contains all nodes between the start and end
  // nodes of the selection.
  std::set<ui::AXNodeID> display_node_ids_;

  // Theme information.
  std::string font_name_ = string_constants::kReadAnythingDefaultFontName;
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
