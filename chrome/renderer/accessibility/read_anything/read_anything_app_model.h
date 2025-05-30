// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_APP_MODEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace ui {
class AXNode;
class AXSelection;
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

  // Enum for logging when we show the empty state.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ReadAnythingEmptyState)
  enum class EmptyState {
    kShown = 0,
    kShownWithSelectionAfter = 1,
    kMaxValue = kShownWithSelectionAfter,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingEmptyState)

  struct AXTreeInfo {
    explicit AXTreeInfo(std::unique_ptr<ui::AXTreeManager> manager);
    AXTreeInfo(const AXTreeInfo&) = delete;
    AXTreeInfo& operator=(const AXTreeInfo&) = delete;
    ~AXTreeInfo();

    // Store AXTrees of web contents in the browser's tab strip as
    // AXTreeManagers.
    std::unique_ptr<ui::AXTreeManager> manager;

    // The UKM source ID of the main frame that sources this AXTree. This is
    // used for metrics collection. Only root AXTrees have this set.
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;

    // Used to keep track of how many selections were made for the
    // ukm_source_id. Only recorded during the select-to-distill flow (when the
    // empty state page is shown).
    int num_selections = 0;

    // Whether URL information, namely is_docs, has been set.
    bool is_url_information_set = false;

    // Google Docs are different from regular webpages. We want to distill
    // content from the annotated canvas elements, not the main tree. Only root
    // AXTrees have this set.
    bool is_docs = false;

    // Whether the latest tree is a reload of the previous tree. If false, the
    // latest tree is a new page.
    bool is_reload = false;

    // TODO(41496290): Include any information that is associated with a
    // particular AXTree, namely is_pdf. Right now, this is set every time the
    // active ax tree id changes; instead, it should be set once when a new tree
    // is added.
  };

  // Represents a grouping of AXTreeUpdates received in the same accessibility
  // event.
  using Updates = std::vector<ui::AXTreeUpdate>;

  // Updates need to be grouped by the order in which they were received,
  // rather than just a single vector containing all updates from multiple
  // accessibility events. This is so that Unserialize can be called in
  // batches on the group of Updates received from each call to
  // AccessibilityEventReceived. Otherwise, intermediary updates might
  // cause tree inconsistency issues with the final update.
  using PendingUpdates = std::map<ui::AXTreeID, std::vector<Updates>>;

  static constexpr char kEmptyStateHistogramName[] =
      "Accessibility.ReadAnything.EmptyState";

  ReadAnythingAppModel();
  ReadAnythingAppModel(const ReadAnythingAppModel&) = delete;
  ReadAnythingAppModel& operator=(const ReadAnythingAppModel&) = delete;
  ~ReadAnythingAppModel();

  bool requires_distillation() const { return requires_distillation_; }
  void set_requires_distillation(bool requires_distillation) {
    requires_distillation_ = requires_distillation;
  }

  bool requires_post_process_selection() const {
    return requires_post_process_selection_;
  }
  void set_requires_post_process_selection(
      bool requires_post_process_selection) {
    requires_post_process_selection_ = requires_post_process_selection;
  }

  bool reset_draw_timer() const { return reset_draw_timer_; }
  void set_reset_draw_timer(bool value) { reset_draw_timer_ = value; }

  const ui::AXNodeID& last_expanded_node_id() const {
    return last_expanded_node_id_;
  }
  void set_last_expanded_node_id(const ui::AXNodeID& last_expanded_node_id) {
    last_expanded_node_id_ = last_expanded_node_id;
  }
  void reset_last_expanded_node_id() {
    set_last_expanded_node_id(ui::kInvalidAXNodeID);
  }

  bool redraw_required() const { return redraw_required_; }
  void reset_redraw_required() { redraw_required_ = false; }

  int unprocessed_selections_from_reading_mode() {
    return selections_from_reading_mode_;
  }
  void increment_selections_from_reading_mode() {
    ++selections_from_reading_mode_;
  }
  void decrement_selections_from_reading_mode() {
    --selections_from_reading_mode_;
  }

  const std::string& base_language_code() const { return base_language_code_; }
  void SetBaseLanguageCode(std::string base_language_code);

  const std::vector<std::string>& supported_fonts() const {
    return supported_fonts_;
  }

  const std::string& font_name() const { return font_name_; }
  void set_font_name(std::string font_name) {
    font_name_ = std::move(font_name);
  }

  float font_size() const { return font_size_; }

  bool links_enabled() const { return links_enabled_; }
  void set_links_enabled(bool links_enabled) { links_enabled_ = links_enabled; }

  bool images_enabled() const { return images_enabled_; }
  void set_images_enabled(bool images_enabled) {
    images_enabled_ = images_enabled;
  }

  read_anything::mojom::LetterSpacing letter_spacing() const {
    return letter_spacing_;
  }
  void set_letter_spacing(read_anything::mojom::LetterSpacing letter_spacing) {
    letter_spacing_ = letter_spacing;
  }

  read_anything::mojom::LineSpacing line_spacing() const {
    return line_spacing_;
  }
  void set_line_spacing(read_anything::mojom::LineSpacing line_spacing) {
    line_spacing_ = line_spacing;
  }

  read_anything::mojom::Colors color_theme() const { return color_theme_; }
  void set_color_theme(read_anything::mojom::Colors color_theme) {
    color_theme_ = color_theme;
  }

  // Sometimes iframes can return selection objects that have a valid id but
  // aren't in the tree.
  bool has_selection() const {
    return start_.is_valid() && GetAXNode(start_.id);
  }
  ui::AXNodeID start_node_id() const { return start_.id; }
  ui::AXNodeID end_node_id() const { return end_.id; }
  int start_offset() const { return start_.offset; }
  int end_offset() const { return end_.offset; }

  bool distillation_in_progress() const { return distillation_in_progress_; }
  void set_distillation_in_progress(bool distillation_in_progress) {
    distillation_in_progress_ = distillation_in_progress;
  }

  // The following methods are used for the screen2x data collection pipeline.
  // They all have CHECKs to ensure that the DataCollectionModeForScreen2x
  // feature flag is enabled.
  bool ScreenAIServiceReadyForDataCollection() const;
  void SetScreenAIServiceReadyForDataCollection();
  bool PageFinishedLoadingForDataCollection() const;
  void SetDataCollectionForScreen2xCallback(
      base::OnceCallback<void()> callback);

  bool page_finished_loading() const { return page_finished_loading_; }
  void set_page_finished_loading(bool page_finished_loading) {
    page_finished_loading_ = page_finished_loading;
  }

  bool requires_tree_lang() const { return requires_tree_lang_; }
  void set_requires_tree_lang(bool requires_tree_lang) {
    requires_tree_lang_ = requires_tree_lang;
  }

  bool will_hide() const { return will_hide_; }
  void set_will_hide(bool will_hide) { will_hide_ = will_hide; }

  const std::vector<ui::AXNodeID>& content_node_ids() const {
    return content_node_ids_;
  }

  const std::set<ui::AXNodeID>& display_node_ids() const {
    return display_node_ids_;
  }

  const std::set<ui::AXNodeID>& selection_node_ids() const {
    return selection_node_ids_;
  }

  bool is_empty() const {
    return display_node_ids_.empty() && selection_node_ids_.empty();
  }

  const ui::AXTreeID& active_tree_id() const { return active_tree_id_; }
  void SetActiveTreeId(ui::AXTreeID active_tree_id);
  void SetRootTreeId(ui::AXTreeID root_tree_id);

  ukm::SourceId GetUkmSourceId() const;
  void SetUkmSourceIdForTree(const ui::AXTreeID& tree,
                             ukm::SourceId ukm_source_id);

  int GetNumSelections() const;
  void SetNumSelections(int num_selections);
  void SetTreeInfoUrlInformation(AXTreeInfo& tree_info);
  void SetUrlInformationCallback(base::OnceCallback<void()> callback);
  bool IsDocs() const;
  bool IsReload() const;

  const std::set<ui::AXNodeID>* GetCurrentlyVisibleNodes() const;

  ui::AXNode* GetAXNode(const ui::AXNodeID& ax_node_id) const;

  // Inserts `id` into `non_ignored_ids` if it corresponds to a node that should
  // not be ignored during content distillation. Nodes may be ignored for
  // various reasons, such as being synthetic markers of some type or (some
  // kinds of) interactive elements.
  void InsertIdIfNotIgnored(ui::AXNodeID id,
                            std::set<ui::AXNodeID>& non_ignored_ids);

  bool NodeIsContentNode(ui::AXNodeID ax_node_id) const;

  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      std::string font_name,
      double font_size,
      bool links_enabled,
      bool images_enabled,
      read_anything::mojom::Colors color);

  void OnScroll(bool on_selection, bool from_reading_mode) const;

  void Reset(std::vector<ui::AXNodeID> content_node_ids);

  // Helper functions for the rendering algorithm. Post-process the AXTree and
  // cache values before sending an `updateContent` notification to the Read
  // Anything app.ts.
  //
  // Computes selection nodes from the user's selection. The selection nodes
  // list is only populated when the user's selection contains nodes outside of
  // the display nodes list. By keeping two separate lists of nodes, we can
  // switch back to displaying the default distilled content without recomputing
  // the nodes when the user clears their selection or selects content inside
  // the distilled content.
  bool PostProcessSelection();

  // Computes display nodes from the content nodes. These display nodes will be
  // displayed in the Read Anything app.ts by default.
  void ComputeDisplayNodeIdsForDistilledTree();

  ui::AXSerializableTree* GetActiveTree() const;

  bool ContainsTree(const ui::AXTreeID& tree_id) const;

  bool ContainsActiveTree() const;

  void UnserializePendingUpdates(const ui::AXTreeID& tree_id);

  void ClearPendingUpdates();

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  Updates& updates,
                                  std::vector<ui::AXEvent>& events,
                                  bool speech_playing);

  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id);

  const PendingUpdates& pending_updates_for_testing() const {
    return pending_updates_;
  }

  const std::map<ui::AXTreeID, std::unique_ptr<AXTreeInfo>>&
  tree_infos_for_testing() const {
    return tree_infos_;
  }

  void AdjustTextSize(int increment);
  void ResetTextSize();

  // PDF handling.
  bool is_pdf() const { return is_pdf_; }
  void set_is_pdf(bool is_pdf) { is_pdf_ = is_pdf; }

  void AddObserver(ModelObserver* observer);
  void RemoveObserver(ModelObserver* observer);

  // TODO: crbug.com/416483312 - Longer term, reading mode should support
  // distilling from multiple trees, if they have important content.
  // Currently, reading mode only distills from a child tree if the root tree
  // has no distillable content.

  // Signal if reading mode should allow use of child trees for the active tree
  // if the web content's root AXTree has no distillable content.
  void AllowChildTreeForActiveTree(bool use_child_tree);

 private:
  struct SelectionEndpoint {
    enum class Source {
      kAnchor,
      kFocus,
    };

    SelectionEndpoint() = default;
    SelectionEndpoint(const ui::AXSelection& selection, Source source);

    constexpr bool operator==(const SelectionEndpoint&) const = default;

    constexpr bool is_valid() const { return id != ui::kInvalidAXNodeID; }

    ui::AXNodeID id = ui::kInvalidAXNodeID;
    int offset = -1;
  };

  ui::AXSerializableTree* GetTreeFromId(const ui::AXTreeID& tree_id) const;

  void ResetSelection();

  bool ContentNodesOnlyContainHeadings();

  void AddPendingUpdates(const ui::AXTreeID& tree_id, Updates& updates);

  void UnserializeUpdates(Updates& updates, const ui::AXTreeID& tree_id);

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

  void OnPageLoadTimerTriggered();
  void OnTreeChangeTimerTriggered();

  void SetFontSize(double font_size, int increment = 0);
  void SetUkmSourceId(ukm::SourceId ukm_source_id);

  // State.
  std::map<ui::AXTreeID, std::unique_ptr<AXTreeInfo>> tree_infos_;

  // The AXTreeID of the currently active web contents. For PDFs, this will
  // always be the AXTreeID of the main web contents (not the PDF iframe or its
  // child).
  ui::AXTreeID active_tree_id_ = ui::AXTreeIDUnknown();

  // The AXTreeID of the root tree of the web contents. This will be the same
  // as active_tree_id_ unless root_tree_id_ has no distillable content but has
  // a child tree with distillable content.
  ui::AXTreeID root_tree_id_ = ui::AXTreeIDUnknown();

  // For determining whether the latest tree is a reload or new page.
  std::string previous_tree_url_;
  base::OnceCallback<void()> set_url_information_callback_;

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
  PendingUpdates pending_updates_;

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

  bool redraw_required_ = false;
  ui::AXNodeID last_expanded_node_id_ = ui::kInvalidAXNodeID;

  // Cached set of fonts that support `base_language_code_`, updated whenever
  // that is changed.
  std::vector<std::string> supported_fonts_ =
      GetSupportedFonts(base_language_code_);

  // Theme information.
  std::string font_name_ = supported_fonts_.front();
  float font_size_;
  bool links_enabled_ = true;
  bool images_enabled_ = false;
  read_anything::mojom::LetterSpacing letter_spacing_ =
      read_anything::mojom::LetterSpacing::kDefaultValue;
  read_anything::mojom::LineSpacing line_spacing_ =
      read_anything::mojom::LineSpacing::kDefaultValue;
  read_anything::mojom::Colors color_theme_ =
      read_anything::mojom::Colors::kDefaultValue;

  // Invariant: Either both endpoints are `!is_valid()`, or they are both valid
  // and non-equal.
  SelectionEndpoint start_;
  SelectionEndpoint end_;

  bool requires_distillation_ = false;
  bool reset_draw_timer_ = false;
  bool requires_post_process_selection_ = false;
  int selections_from_reading_mode_ = 0;

  // For screen2x data collection, Chrome is launched from the CLI to open one
  // webpage. We record the result of the distill() call for this entire
  // webpage, so we only make the call once the webpage finished loading and
  // screen ai has loaded.
  bool screen_ai_service_ready_for_data_collection_ = false;
  bool waiting_for_page_load_completion_timer_trigger_ = true;
  bool waiting_for_tree_change_timer_trigger_ = true;
  base::OneShotTimer timer_since_page_load_for_data_collection_;
  base::RetainingOneShotTimer timer_since_tree_changed_for_data_collection_;
  base::OnceCallback<void()> data_collection_for_screen2x_callback_;

  // Whether the webpage has finished loading or not.
  bool page_finished_loading_ = false;

  // If the page language can't be determined by the model, we can check the
  // AX tree to see if it has that information, but the ax tree is created
  // asynchronously from the language determination so we need to keep track of
  // that here.
  bool requires_tree_lang_ = false;

  bool will_hide_ = false;

  std::map<ui::AXTreeID, ukm::SourceId> pending_ukm_sources_;

  // Possible child tree ids that could be used to distill content if the
  // root tree has no distillable content. This will only be used if
  // may_use_child_for_active_tree_ is true.
  std::set<ui::AXTreeID> child_tree_ids_;

  // If reading mode should attempt to use child trees to distill content. This
  // should only be true if the root tree has no distillable content.
  bool may_use_child_for_active_tree_ = false;

  // List of observers of model state changes.
  base::ObserverList<ModelObserver, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<ReadAnythingAppModel> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_APP_MODEL_H_
