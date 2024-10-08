// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include <map>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/renderer/accessibility/read_aloud_traversal_utils.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace ui {
class AXNode;
class AXSerializableTree;
}  // namespace ui

// A class that holds state for the ReadAnythingAppController for the Read
// Anything WebUI app.
class ReadAnythingAppModel {
 public:
  // Allows one to observer changes in the model state.
  class ModelObserver : public base::CheckedObserver {
   public:
    virtual void OnTreeAdded(ui::AXTree* tree) = 0;
    virtual void OnTreeRemoved(ui::AXTree* tree) = 0;
  };

  ReadAnythingAppModel();
  ~ReadAnythingAppModel();
  ReadAnythingAppModel(const ReadAnythingAppModel& other) = delete;
  ReadAnythingAppModel& operator=(const ReadAnythingAppModel&) = delete;

  struct AXTreeInfo {
    explicit AXTreeInfo(std::unique_ptr<ui::AXTreeManager> other);
    ~AXTreeInfo();
    AXTreeInfo(const AXTreeInfo& other) = delete;
    AXTreeInfo& operator=(const AXTreeInfo&) = delete;

    // Store AXTrees of web contents in the browser's tab strip as
    // AXTreeManagers.
    std::unique_ptr<ui::AXTreeManager> manager;

    // The UKM source ID of the main frame that sources this AXTree. This is
    // used for metrics collection. Only root AXTrees have this set.
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;

    // Used to keep track of how many selections were made for the
    // ukm_source_id. Only recorded during the select-to-distill flow (when the
    // empty state page is shown).
    int32_t num_selections = 0;

    // Whether URL information, namely is_docs, has been set.
    bool is_url_information_set = false;

    // Google Docs are different from regular webpages. We want to distill
    // content from the annotated canvas elements, not the main tree. Only root
    // AXTrees have this set.
    bool is_docs = false;

    // TODO(41496290): Include any information that is associated with a
    // particular AXTree, namely is_pdf. Right now, this is set every time the
    // active ax tree id changes; instead, it should be set once when a new tree
    // is added.
  };

  bool requires_distillation() const { return requires_distillation_; }
  void set_requires_distillation(bool value) { requires_distillation_ = value; }
  bool requires_post_process_selection() const {
    return requires_post_process_selection_;
  }
  void set_requires_post_process_selection(bool value) {
    requires_post_process_selection_ = value;
  }
  bool reset_draw_timer() const { return reset_draw_timer_; }
  void set_reset_draw_timer(bool value) { reset_draw_timer_ = value; }

  const ui::AXNodeID& last_expanded_node_id() const {
    return last_expanded_node_id_;
  }

  void set_last_expanded_node_id(const ui::AXNodeID& node_id) {
    last_expanded_node_id_ = node_id;
  }

  void reset_last_expanded_node_id() {
    set_last_expanded_node_id(ui::kInvalidAXNodeID);
  }

  bool redraw_required() const { return redraw_required_; }
  void reset_redraw_required() { redraw_required_ = false; }
  bool selection_from_action() const { return selection_from_action_; }
  void set_selection_from_action(bool value) { selection_from_action_ = value; }

  const std::string& base_language_code() const { return base_language_code_; }

  void SetBaseLanguageCode(const std::string& code);

  std::vector<std::string> GetSupportedFonts();

  // Theme
  const std::string& font_name() const { return font_name_; }
  void set_font_name(const std::string& font) { font_name_ = font; }
  float font_size() const { return font_size_; }
  void set_font_size(float font_size) { font_size_ = font_size; }
  bool links_enabled() const { return links_enabled_; }
  bool images_enabled() const { return images_enabled_; }
  int letter_spacing() const { return letter_spacing_; }
  void set_letter_spacing(int letter_spacing) {
    letter_spacing_ = letter_spacing;
  }
  int line_spacing() const { return line_spacing_; }
  void set_line_spacing(int line_spacing) { line_spacing_ = line_spacing; }
  int color_theme() const { return color_theme_; }
  void set_color_theme(int color_theme) { color_theme_ = color_theme; }

  // Selection.
  bool has_selection() const { return has_selection_; }
  const ui::AXNodeID& start_node_id() const { return start_node_id_; }
  const ui::AXNodeID& end_node_id() const { return end_node_id_; }
  int32_t start_offset() const { return start_offset_; }
  int32_t end_offset() const { return end_offset_; }

  bool distillation_in_progress() const { return distillation_in_progress_; }
  bool is_empty() const {
    return display_node_ids_.empty() && selection_node_ids_.empty();
  }

  // The following methods are used for the screen2x data collection pipeline.
  // They all have CHECKs to ensure that the DataCollectionModeForScreen2x
  // feature flag is enabled.
  bool ScreenAIServiceReadyForDataColletion() const;
  void SetScreenAIServiceReadyForDataColletion(bool value);
  bool PageFinishedLoadingForDataCollection() const;
  void SetPageFinishedLoadingForDataCollection(bool value);
  void SetDataCollectionForScreen2xCallback(
      base::RepeatingCallback<void()> callback);

  bool page_finished_loading() const { return page_finished_loading_; }
  void set_page_finished_loading(bool value) {
    page_finished_loading_ = value;
  }
  bool requires_tree_lang() const { return requires_tree_lang_; }
  void set_requires_tree_lang(bool value) { requires_tree_lang_ = value; }

  const std::vector<ui::AXNodeID>& content_node_ids() const {
    return content_node_ids_;
  }
  const std::set<ui::AXNodeID>& display_node_ids() const {
    return display_node_ids_;
  }
  const std::set<ui::AXNodeID>& selection_node_ids() const {
    return selection_node_ids_;
  }

  const ui::AXTreeID& active_tree_id() const { return active_tree_id_; }
  void SetActiveTreeId(const ui::AXTreeID& active_tree_id);

  void set_distillation_in_progress(bool distillation) {
    distillation_in_progress_ = distillation;
  }

  const ukm::SourceId& UkmSourceId();
  void SetUkmSourceId(const ukm::SourceId ukm_source_id);
  int32_t NumSelections();
  void SetNumSelections(const int32_t& num_selections);

  void AddUrlInformationForTreeId(const ui::AXTreeID& tree_id);
  bool IsDocs() const;

  ui::AXNode* GetAXNode(const ui::AXNodeID& ax_node_id) const;
  bool NodeIsContentNode(const ui::AXNodeID& ax_node_id) const;
  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      const std::string& font,
      double font_size,
      bool links_enabled,
      bool images_enabled,
      read_anything::mojom::Colors color);
  void OnScroll(bool on_selection, bool from_reading_mode) const;
  void OnSelection(ax::mojom::EventFrom event_from);

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

  ui::AXSerializableTree* GetTreeFromId(const ui::AXTreeID& tree_id) const;
  void AddTree(const ui::AXTreeID& tree_id,
               std::unique_ptr<ui::AXSerializableTree> tree);

  bool ContainsTree(const ui::AXTreeID& tree_id) const;

  void UnserializePendingUpdates(const ui::AXTreeID& tree_id);

  void ClearPendingUpdates();

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  std::vector<ui::AXTreeUpdate>& updates,
                                  std::vector<ui::AXEvent>& events,
                                  const bool speech_playing);

  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id);

  std::map<ui::AXTreeID, std::vector<ui::AXTreeUpdate>>&
  GetPendingUpdatesForTesting();

  std::map<ui::AXTreeID, std::unique_ptr<ReadAnythingAppModel::AXTreeInfo>>*
  GetTreesForTesting();

  void EraseTreeForTesting(const ui::AXTreeID& tree_id);

  double GetLineSpacingValue(
      read_anything::mojom::LineSpacing line_spacing) const;
  double GetLetterSpacingValue(
      read_anything::mojom::LetterSpacing letter_spacing) const;

  void IncreaseTextSize();
  void DecreaseTextSize();
  void ResetTextSize();
  void ToggleLinksEnabled();
  void ToggleImagesEnabled();

  // PDF handling.
  void set_is_pdf(bool is_pdf) { is_pdf_ = is_pdf; }
  bool is_pdf() const { return is_pdf_; }

  void AddObserver(ModelObserver* observer);
  void RemoveObserver(ModelObserver* observer);

 private:
  void EraseTree(const ui::AXTreeID& tree_id);

  void InsertDisplayNode(const ui::AXNodeID& node);
  void ResetSelection();
  void InsertSelectionNode(const ui::AXNodeID& node);
  void UpdateSelection();
  void ComputeSelectionNodeIds();
  bool IsCurrentSelectionEmpty();
  bool SelectionInsideDisplayNodes();
  bool ContentNodesOnlyContainHeadings();

  void AddPendingUpdates(const ui::AXTreeID& tree_id,
                         std::vector<ui::AXTreeUpdate>& updates);

  void UnserializeUpdates(std::vector<ui::AXTreeUpdate>& updates,
                          const ui::AXTreeID& tree_id);

  void ProcessNonGeneratedEvents(const std::vector<ui::AXEvent>& events);

  // The tree size arguments are used to determine if distillation of a PDF is
  // necessary.
  void ProcessGeneratedEvents(const ui::AXEventGenerator& event_generator,
                              size_t prev_tree_size,
                              size_t tree_size);

  // Runs the data collection for screen2x pipeline, provided in the form of a
  // callback from the ReadAnythingAppController. This should only be called
  // when the DataCollectionModeForScreen2x feature is enabled.
  void MaybeRunDataCollectionForScreen2xCallback();

  // State.
  std::map<ui::AXTreeID, std::unique_ptr<ReadAnythingAppModel::AXTreeInfo>>
      tree_infos_;

  // The AXTreeID of the currently active web contents. For PDFs, this will
  // always be the AXTreeID of the main web contents (not the PDF iframe or its
  // child).
  ui::AXTreeID active_tree_id_ = ui::AXTreeIDUnknown();

  // PDFs are handled differently than regular webpages. That is because they
  // are stored in a different web contents and the actual PDF text is inside an
  // iframe. In order to get tree information from the PDF web contents, we need
  // to enable accessibility on it first. Then, we will get tree updates from
  // the iframe to send to the distiller.
  // This is the flow:
  //    main web contents -> pdf web contents -> iframe
  // In accessibility terms:
  //    AXTree -(via child tree)-> AXTree -(via child tree)-> AXTree
  // The last AXTree is the one we want to send to the distiller since it
  // contains the PDF text.
  bool is_pdf_ = false;

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

  // The current base language code used for fonts or reading aloud.
  std::string base_language_code_ = "en";
  std::map<ui::AXNodeID, std::string> aria_expanded_node_states_;

  bool redraw_required_ = false;
  ui::AXNodeID last_expanded_node_id_ = ui::kInvalidAXNodeID;

  // Theme information.
  std::string font_name_ = string_constants::kReadAnythingPlaceholderFontName;
  float font_size_ = kReadAnythingDefaultFontScale;
  bool links_enabled_ = kReadAnythingDefaultLinksEnabled;
  bool images_enabled_ = kReadAnythingDefaultImagesEnabled;
  int letter_spacing_ = (int)read_anything::mojom::LetterSpacing::kDefaultValue;
  int line_spacing_ = (int)read_anything::mojom::LineSpacing::kDefaultValue;
  int color_theme_ = (int)read_anything::mojom::Colors::kDefaultValue;

  // Selection information.
  bool has_selection_ = false;
  ui::AXNodeID start_node_id_ = ui::kInvalidAXNodeID;
  ui::AXNodeID end_node_id_ = ui::kInvalidAXNodeID;
  int32_t start_offset_ = -1;
  int32_t end_offset_ = -1;
  bool requires_distillation_ = false;
  bool reset_draw_timer_ = false;
  bool requires_post_process_selection_ = false;
  bool selection_from_action_ = false;

  // For screen2x data collection, Chrome is launched from the CLI to open one
  // webpage. We record the result of the distill() call for this entire
  // webpage, so we only make the call once the webpage finished loading and
  // screen ai has loaded.
  bool ScreenAIServiceReadyForDataColletion_ = false;
  bool PageFinishedLoadingForDataCollection_ = false;
  base::OneShotTimer timer_since_page_load_for_data_collection_;
  base::RetainingOneShotTimer timer_since_tree_changed_for_data_collection_;
  base::RepeatingCallback<void()> data_collection_for_screen2x_callback_;

  // Whether the webpage has finished loading or not.
  bool page_finished_loading_ = false;

  // Maps fonts to whether the current base_language_code_ supports that font.
  std::map<std::string_view, bool> supported_fonts_;
  // If the page language can't be determined by the model, we can check the
  // AX tree to see if it has that information, but the ax tree is created
  // asynchronously from the language determination so we need to keep track of
  // that here.
  bool requires_tree_lang_ = false;

  // List of observers of model state changes.
  base::ObserverList<ModelObserver, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<ReadAnythingAppModel> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
