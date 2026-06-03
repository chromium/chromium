// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_APP_MODEL_H_

#include <algorithm>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/common/read_anything/read_anything.mojom-shared.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/accessibility_features.h"
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

  // Enum for keeping track of the current distillation method being used by the
  // ReadAnythingAppController.
  enum class DistillationMethod {
    kScreen2x = 0,
    kReadability = 1,
    kMaxValue = kReadability,
  };

  // Enum for tracking the side panel's distillation mode. This determines
  // whether the view is derived from the main content article or from
  // a specific user selection in the main panel.
  // TODO: crbug.com/503024139 - Track when we switch modes.
  enum class SidePanelDistillationMode {
    kMainContent = 0,
    kSelection = 1,
    kMaxValue = kSelection,
  };

  // Enum for logging selection attempts before Readability mapping is complete.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ReadAnythingEarlySelection)
  enum class EarlySelection {
    kSidePanelSelection = 0,
    kMainPanelSelection = 1,
    kMaxValue = kMainPanelSelection,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingEarlySelection)

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

    // Whether the latest tree is the "What's New" page.
    bool is_whats_new = false;

    // TODO(41496290): Include any information that is associated with a
    // particular AXTree, namely is_pdf. Right now, this is set every time the
    // active ax tree id changes; instead, it should be set once when a new tree
    // is added.
  };

  // Stores the necessary information to determine if one link is different
  // from another when interacting with a link from Readability distilled
  // content.
  struct AnchorData {
    AnchorData();
    ~AnchorData();
    AnchorData(const AnchorData& other);
    AnchorData& operator=(const AnchorData& other);

    // The value of the HTML 'id' attribute (e.g., <a id="my-link">).
    std::string html_id;

    // The accessible name or visible text of the link, used by screen readers.
    std::string name;

    // The HTML 'target' attribute, indicating where to open the link (e.g.,
    // "_blank").
    std::string target;

    // The text content of the node immediately following this link.
    std::string text_after;

    // The text content of the node immediately preceding this link.
    std::string text_before;

    // The HTML 'title' attribute, typically shown as a hover tooltip.
    std::string title;

    ui::AXNodeID id;
  };

  // Represents a segment of text within an AXNode. A distilled Readability
  // block may be mapped to multiple AXNodes (e.g., a paragraph with a link
  // inside); each part of that mapping is a MappingSegment.
  struct MappingSegment {
    ui::AXNodeID id;
    // The 0-based start and end character offsets within the *Readability
    // block's* text content that correspond to this AXNode.
    int start;
    int end;
    // The 0-based character offset within the *AXNode's* own text content where
    // this segment begins.
    int ax_node_offset;
  };

  // Represents a segment of the flattened |global_ax_tree_text_| and the AXNode
  // it comes from. This metadata allows the mapping algorithm for text
  // selection to translate a character range found in |global_ax_tree_text_|
  // back into its original AXNode and relative offset.
  struct AXNodeSegment {
    ui::AXNodeID id;
    std::u16string text;
    // The 0-based starting index of this node's text within the
    // concatenated |global_ax_tree_text_| string.
    size_t start_offset;
  };

  // Represents a grouping of AXTreeUpdates received in the same accessibility
  // event.
  using Updates = std::vector<ui::AXTreeUpdate>;

  // Updates need to be grouped by the order in which they were received,
  // rather than just a single vector containing all updates from multiple
  // accessibility events. This is so that Unserialize can be called in
  // batches on the group of Updates received from each call to
  // ApplyAccessibilityUpdates. Otherwise, intermediary updates might
  // cause tree inconsistency issues with the final update.
  using PendingUpdates = std::map<ui::AXTreeID, std::vector<Updates>>;

  static constexpr char kEmptyStateHistogramName[] =
      "Accessibility.ReadAnything.EmptyState";

  static constexpr char kEarlySelectionHistogramName[] =
      "Accessibility.ReadAnything.Readability.EarlySelection";

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

  bool reset_distillation_delay_timer() const {
    return reset_distillation_delay_timer_;
  }
  void set_reset_distillation_delay_timer(bool reset) {
    reset_distillation_delay_timer_ = reset;
  }

  int unprocessed_selections_from_reading_mode() {
    return selections_from_reading_mode_;
  }
  void increment_selections_from_reading_mode() {
    ++selections_from_reading_mode_;
  }
  void decrement_selections_from_reading_mode() {
    --selections_from_reading_mode_;
  }
  int words_seen() const { return words_seen_; }
  void set_words_seen(const int words_seen) { words_seen_ = words_seen; }
  int words_heard() const { return words_heard_; }
  void set_words_heard(const int words_heard) { words_heard_ = words_heard; }
  int words_distilled() const { return words_distilled_; }
  void set_words_distilled(const int words_distilled) {
    words_distilled_ = words_distilled;
  }

  std::optional<base::TimeTicks> line_focus_session_start_time() const {
    return line_focus_session_start_time_;
  }
  void set_line_focus_session_start_time(const base::TimeTicks time) {
    line_focus_session_start_time_ = time;
  }
  int line_focus_mouse_distance() const { return line_focus_mouse_distance_; }
  void set_line_focus_mouse_distance(const int distance) {
    line_focus_mouse_distance_ = distance;
  }
  int line_focus_scroll_distance() const { return line_focus_scroll_distance_; }
  void set_line_focus_scroll_distance(const int distance) {
    line_focus_scroll_distance_ = distance;
  }
  int line_focus_keyboard_lines() const { return line_focus_keyboard_lines_; }
  void set_line_focus_keyboard_lines(const int lines) {
    line_focus_keyboard_lines_ = lines;
  }
  int line_focus_speech_lines() const { return line_focus_speech_lines_; }
  void set_line_focus_speech_lines(const int lines) {
    line_focus_speech_lines_ = lines;
  }
  void ResetLineFocusSession();

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

  // Returns the distillation method that produced the content currently
  // visible in the UI. This is used by the WebUI to correctly interpret
  // and render the current model data.
  DistillationMethod current_content_distillation_method() const {
    return current_content_distillation_method_;
  }
  void set_current_content_distillation_method(DistillationMethod method) {
    current_content_distillation_method_ = method;
  }

  // Returns the distillation method that will be used for the next content
  // update. Note: For Readability, distillation occurs in the browser process,
  // so this represents the source of the next content update we will receive.
  DistillationMethod next_distillation_method() const {
    return next_distillation_method_;
  }
  void set_next_distillation_method(DistillationMethod method) {
    next_distillation_method_ = method;
  }
  bool is_readability_next_distillation_method() const {
    return next_distillation_method() == DistillationMethod::kReadability;
  }
  bool is_readability_current_distillation_method() const {
    return current_content_distillation_method_ ==
           DistillationMethod::kReadability;
  }
  bool should_apply_accessibility_updates_for_readability() const {
    // Accessibility updates for Readability shouldn't be applied outside
    // of the regular accessibility update process if Readability is disabled.
    if (!features::IsReadAnythingWithReadabilityEnabled()) {
      return false;
    }

    if (is_readability_next_distillation_method() &&
        distillation_state_ ==
            read_anything::mojom::ReadAnythingDistillationState::
                kDistillationInProgress) {
      return true;
    }

    if (is_readability_current_distillation_method() &&
        distillation_state_ ==
            read_anything::mojom::ReadAnythingDistillationState::
                kDistillationWithContent) {
      return true;
    }

    return false;
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

  read_anything::mojom::LineFocus last_non_disabled_line_focus() const {
    return last_non_disabled_line_focus_;
  }
  void set_last_non_disabled_line_focus(
      read_anything::mojom::LineFocus last_non_disabled_line_focus) {
    last_non_disabled_line_focus_ = last_non_disabled_line_focus;
  }

  bool line_focus_enabled() const { return line_focus_enabled_; }
  void set_line_focus_enabled(bool line_focus_enabled) {
    line_focus_enabled_ = line_focus_enabled;
  }

  // Sometimes iframes can return selection objects that have a valid id but
  // aren't in the tree.
  bool has_selection() const {
    if (IsWhatsNew()) {
      return start_.is_valid() && end_.is_valid() &&
             GetAXNodeFromRoot(start_.id) && GetAXNodeFromRoot(end_.id);
    }
    return start_.is_valid() && end_.is_valid() && GetAXNode(start_.id) &&
           GetAXNode(end_.id);
  }
  ui::AXNodeID start_node_id() const { return start_.id; }
  ui::AXNodeID end_node_id() const { return end_.id; }
  int start_offset() const { return start_.offset; }
  int end_offset() const { return end_.offset; }

  bool screen2x_distiller_running() const {
    return screen2x_distiller_running_;
  }
  void set_screen2x_distiller_running(bool screen2x_distiller_running) {
    screen2x_distiller_running_ = screen2x_distiller_running;
  }

  bool should_extract_anchors_from_tree_for_readability() const {
    return should_extract_anchors_from_tree_for_readability_;
  }
  void set_should_extract_anchors_from_tree_for_readability(
      bool should_extract_anchors_from_tree_for_readability) {
    should_extract_anchors_from_tree_for_readability_ =
        should_extract_anchors_from_tree_for_readability;
  }

  // Processes the tree anchors.
  // Returns true indicating that the tree was successfully processed and we can
  // notify the frontend that anchors are ready.
  bool ProcessAXTreeAnchors();
  void ResetAXTreeAnchors();
  const std::map<std::string, std::vector<AnchorData>>& ax_tree_anchors()
      const {
    return ax_tree_anchors_;
  }

  const std::vector<std::u16string>& readability_text_blocks() const {
    return readability_text_blocks_;
  }
  void set_readability_text_blocks(std::vector<std::u16string> blocks) {
    readability_text_blocks_ = std::move(blocks);
  }

  bool should_map_rendered_text_to_tree_for_readability() const {
    return should_map_rendered_text_to_tree_for_readability_;
  }
  void set_should_map_rendered_text_to_tree_for_readability(
      bool should_map_rendered_text_to_tree_for_readability) {
    should_map_rendered_text_to_tree_for_readability_ =
        should_map_rendered_text_to_tree_for_readability;
  }

  const std::vector<std::vector<MappingSegment>>& text_to_ax_map() const {
    return text_to_ax_map_;
  }

  // Maps the distilled rendered text from the WebUI with the AXtree.
  // Returns true if the AXtree was successfully processed and we can
  // notify the frontend that the mapping is ready.
  bool MapRenderedTextToTree(const std::vector<std::u16string>& blocks);

  // Returns the AX mapping for the given index. This is the primary interface
  // for the WebUI to consume the results of the mapping algorithm. The index
  // corresponds to the index of the block in readability_text_blocks_ and
  // text_to_ax_map_.
  std::vector<MappingSegment> GetAXMapping(size_t index) const;

  const std::u16string& global_ax_tree_text() const {
    return global_ax_tree_text_;
  }
  const std::vector<AXNodeSegment>& flattened_ax_tree_nodes() const {
    return flattened_ax_tree_nodes_;
  }

  bool is_readability_mapping_in_progress() const {
    return is_readability_mapping_in_progress_;
  }
  void set_is_readability_mapping_in_progress(bool ready) {
    is_readability_mapping_in_progress_ = ready;
  }

  bool has_logged_early_selection() const {
    return has_logged_early_selection_;
  }
  void set_has_logged_early_selection(bool logged) {
    has_logged_early_selection_ = logged;
  }

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
  const ui::AXTreeID& root_tree_id() const { return root_tree_id_; }

  ukm::SourceId GetUkmSourceId() const;
  void SetUkmSourceIdForTree(const ui::AXTreeID& tree,
                             ukm::SourceId ukm_source_id);

  int GetNumSelections() const;
  void SetNumSelections(int num_selections);
  void SetTreeInfoUrlInformation(AXTreeInfo& tree_info);
  void SetUrlInformationCallback(base::OnceCallback<void()> callback);
  bool IsDocs() const;
  bool IsReload() const;
  bool IsWhatsNew() const;

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
      read_anything::mojom::Colors color,
      read_anything::mojom::LineFocus last_non_disabled_line_focus,
      bool line_focus_enabled);

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

  // Synchronizes the model's selection endpoints (start_ and end_) with the
  // latest state of the active accessibility tree. Ensures that the endpoints
  // are stored in forward tree order.
  void UpdateSelectionEndpoints();

  // Returns true if the user's current selection is entirely contained within
  // the distilled article content (display_node_ids_).
  bool IsSelectionInDistilledContent() const;

  // Traverses the accessibility tree to populate |selection_node_ids_| with
  // the nodes required to render a custom user selection in the side panel.
  void ComputeSelectionNodeIdsForSelectionMode();

  // Computes display nodes from the content nodes. These display nodes will be
  // displayed in the Read Anything app.ts by default.
  void ComputeDisplayNodeIdsForDistilledTree();

  ui::AXSerializableTree* GetActiveTree() const;

  // Returns the active AXTree if it is available, initialized, and contains a
  // root node. Returns nullptr otherwise.
  ui::AXSerializableTree* GetValidActiveTree() const;

  bool ContainsTree(const ui::AXTreeID& tree_id) const;

  bool ContainsActiveTree() const;

  void UnserializePendingUpdates(const ui::AXTreeID& tree_id);

  void ClearPendingUpdates();

  // Applies accessibility updates to the AXTree with the given tree_id.
  // Unserializes the updates, processes generated events, and updates the
  // model's state.
  void ApplyAccessibilityUpdates(const ui::AXTreeID& tree_id,
                                 Updates& updates,
                                 std::vector<ui::AXEvent>& events);

  // Queues accessibility updates to be processed later. This is used when
  // the controller decides to not process the updates immediately to avoid
  // interrupting the user experience. The updates are stored in
  // pending_updates_.
  void QueueAccessibilityUpdates(const ui::AXTreeID& tree_id,
                                 Updates& updates,
                                 std::vector<ui::AXEvent>& events);

  // Ensures that the AXTree with the given tree_id exists in the model's
  // tree_infos_ map. If the tree does not exist, a new one is created.
  // Also updates the active tree ID if necessary.
  void PrepareForAXTreeUpdates(const ui::AXTreeID& tree_id);

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
  void SetDefaultDistillationMethod();

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

  bool SelectionNodesContainedInDistilledContent() const;

  read_anything::mojom::ReadAnythingPresentationState
  active_presentation_state() const {
    return active_presentation_state_;
  }
  void set_active_presentation_state(
      read_anything::mojom::ReadAnythingPresentationState
          active_presentation_state) {
    active_presentation_state_ = active_presentation_state;
  }

  read_anything::mojom::ReadAnythingDistillationState distillation_state()
      const {
    return distillation_state_;
  }
  void set_distillation_state(
      read_anything::mojom::ReadAnythingDistillationState distillation_state) {
    distillation_state_ = distillation_state;
  }

  SidePanelDistillationMode side_panel_distillation_mode() const {
    return side_panel_distillation_mode_;
  }

 private:
  // TODO(crbug.com/513618559): Move text selection mapping algorithm logic
  // into static utility methods / class.
  // An index for searching substrings within the flattened
  // |global_ax_tree_text_|. It uses a Suffix Array to find any string in O(log
  // N) time, where N is the length of the article.
  struct SuffixArray {
    SuffixArray();
    ~SuffixArray();

    // A non-owning reference to the text being indexed.
    std::u16string_view text;

    // A list of starting indices of all suffixes in |text|, sorted
    // lexicographically.
    std::vector<uint32_t> suffix_array;

    // Build the index for the given text. This must be called before FindRange.
    void Build(std::u16string_view text);

    // Finds the range of all occurrences of |query| within the indexed text.
    // Returns a pair of iterators into |suffix_array| defining the range
    // [begin, end). The number of occurrences is std::distance(begin, end).
    std::pair<std::vector<uint32_t>::const_iterator,
              std::vector<uint32_t>::const_iterator>
    FindRange(std::u16string_view query) const;
  };

  // A confirmed alignment point between a distilled Readability block and a
  // character range in the global flattened AXTree text.
  struct AlignmentAnchor {
    size_t block_index;
    size_t block_start;
    size_t block_end;
    size_t ax_start;
    size_t ax_end;
  };

  // Represents a specific slice of text within a distilled Readability block
  // that is still waiting to be mapped. Used by GapSubstring alignment to
  // narrow down the search as anchors are "pinned".
  struct TextRange {
    size_t block_index;
    // Character offsets within readability_text_blocks_[block_index].
    size_t start;
    size_t end;

    size_t length() const { return end > start ? end - start : 0; }
    bool empty() const { return length() == 0; }
  };

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

  ui::AXNode* GetAXNodeFromRoot(const ui::AXNodeID& ax_node_id) const;

  void ResetSelection();

  bool ContentNodesOnlyContainHeadings();

  void AddPendingUpdates(const ui::AXTreeID& tree_id, Updates& updates);

  void UnserializeUpdates(const Updates& updates, const ui::AXTreeID& tree_id);

  void ProcessNonGeneratedEvents(const std::vector<ui::AXEvent>& events);

  // The tree size arguments are used to determine if distillation of a PDF is
  // necessary.
  void ProcessGeneratedEvents(const ui::AXEventGenerator& event_generator,
                              size_t prev_tree_size,
                              size_t tree_size);

  void EnsureAXTreeExists(const ui::AXTreeID& tree_id);

  void UpdateActiveTreeIfNeeded(const ui::AXTreeID& tree_id);

  void SetFontSize(double font_size, int increment = 0);
  void SetUkmSourceId(ukm::SourceId ukm_source_id);
  std::map<std::string, std::vector<AnchorData>> CollectAnchorsFromAXTree(
      ui::AXSerializableTree* tree);

  // Identifies blocks that appear exactly once in both the original AXTree
  // and the distilled Readability output.
  std::vector<AlignmentAnchor> FindGloballyUniqueBlocks(
      const std::vector<std::u16string>& blocks,
      const SuffixArray& index,
      const base::flat_map<std::u16string_view, int>& block_counts);

  // Filters alignment anchor candidates using the Longest Increasing
  // Subsequence (LIS) algorithm based on AXTree positions. Establishing a
  // monotonic order is required for the recursive gap alignment step in the
  // text selection mapping algorithm, as it  ensures that the search ranges
  // between anchors are valid and sequential.
  std::vector<AlignmentAnchor> FilterMonotonicAnchors(
      std::vector<AlignmentAnchor> candidates);

  // Recursively fills the gaps between established anchors by searching for the
  // longest common unique substrings.
  void GapSubstringAlignment(const std::vector<std::u16string>& blocks,
                             const SuffixArray& index,
                             const std::vector<AlignmentAnchor>& major_anchors);

  // Converts the gap search space from block indices into unmapped TextRanges
  // to trigger AlignSubstring for the defined gap.
  void AlignGap(const std::vector<std::u16string>& blocks,
                const SuffixArray& index,
                size_t block_start,
                size_t block_end,
                size_t ax_start,
                size_t ax_end);

  // Recursively finds and maps the Longest Locally Unique Common Substring
  // (LULCS) within a specific distilled and AXTree gap.
  void AlignSubstring(const std::vector<std::u16string>& blocks,
                      const SuffixArray& index,
                      std::vector<TextRange> distilled_ranges,
                      size_t ax_start,
                      size_t ax_end);

  // Sequentially maps the longest possible substrings for any remaining text
  // within a gap.
  void AlignRelativeOrder(const std::vector<std::u16string>& blocks,
                          const SuffixArray& index,
                          std::vector<TextRange> distilled_ranges,
                          size_t ax_start,
                          size_t ax_end);

  // Searches for the Longest Locally Unique Common Substring (LULCS) that
  // appears exactly once in the AXTree gap [ax_start, ax_end).
  //
  // The algorithm iterates through unmapped blocks, using binary search on
  // substring length and the Suffix Array index to find global occurrences
  // in O(log N) time. It then filters for local uniqueness within the
  // current gap, pruning candidates shorter than the current best match.
  //
  // Complexity: O(G * L * log L * log N) average case, where G is the number
  // of ranges, L is block length, and N is article length.
  AlignmentAnchor FindLongestLocallyUniqueSubstring(
      const std::vector<std::u16string>& blocks,
      const SuffixArray& index,
      const std::vector<TextRange>& distilled_ranges,
      size_t ax_start,
      size_t ax_end);

  // Identifies all AXNodes that contribute to a given range from
  // |global_ax_tree_text_| and creates MappingSegments with offsets relative to
  // the distilled block.
  // Args:
  //  |ax_start|:  The start index of the match in |global_ax_tree_text|.
  //  |ax_end|:    The end index (exclusive) in |global_ax_tree_text|.
  //  |block_internal_offset|: The starting character position of this match
  // within the rendered Readability block (usually 0 for whole-block matches).
  std::vector<MappingSegment> CreateSegmentsForMatch(
      size_t ax_start,
      size_t ax_end,
      size_t block_internal_offset);

  // Traverses the AXTree to create a flattened text representation for the text
  // selection mapping algorithm.
  void FlattenAXTree(ui::AXSerializableTree* tree);

  // Logs the execution time for each step of the Readability mapping algorithm.
  void RecordReadabilityMappingMetrics(base::TimeDelta total_duration,
                                       base::TimeDelta flattening_duration,
                                       base::TimeDelta suffix_array_duration,
                                       base::TimeDelta initial_anchors_duration,
                                       base::TimeDelta gap_alignment_duration);

  // Checks if a candidate AXTree range overlaps with text that has already
  // been mapped to a distilled block. This prevents multiple mappings to the
  // same page's text.
  bool IsAXRangeOccupied(size_t ax_start, size_t ax_end) const;

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
  bool screen2x_distiller_running_ = false;

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

  read_anything::mojom::LineFocus last_non_disabled_line_focus_ =
      read_anything::mojom::LineFocus::kMediumStaticWindow;
  bool line_focus_enabled_ = false;

  // Invariant: Either both endpoints are `!is_valid()`, or they are both valid
  // and non-equal.
  SelectionEndpoint start_;
  SelectionEndpoint end_;

  bool requires_distillation_ = false;
  bool reset_draw_timer_ = false;
  bool requires_post_process_selection_ = false;
  int selections_from_reading_mode_ = 0;
  int words_seen_ = 0;
  int words_heard_ = 0;
  int words_distilled_ = 0;

  // Line focus session information. Used for logging.
  std::optional<base::TimeTicks> line_focus_session_start_time_;
  int line_focus_mouse_distance_ = 0;
  int line_focus_scroll_distance_ = 0;
  int line_focus_keyboard_lines_ = 0;
  int line_focus_speech_lines_ = 0;

  // Whether the webpage has finished loading or not.
  bool page_finished_loading_ = false;

  // If the page language can't be determined by the model, we can check the
  // AX tree to see if it has that information, but the ax tree is created
  // asynchronously from the language determination so we need to keep track of
  // that here.
  bool requires_tree_lang_ = false;

  bool will_hide_ = false;

  // Whether the distillation delay timer should be reset.
  bool reset_distillation_delay_timer_ = false;

  // Whether we should traverse the tree to find all the anchors on it.
  bool should_extract_anchors_from_tree_for_readability_;
  // Holds a map of an URL string with all the AX Tree Nodes that are related
  // to that specific URL.
  std::map<std::string, std::vector<AnchorData>> ax_tree_anchors_;

  // Holds the text blocks extracted from the current rendered distillation. A
  // "block" represents the textContent of a non-whitespace DOM text node from
  // the WebUI. Blocks are used as one of the inputs along with the AXTree for
  // the select text mapping algorithm. This algorithm maps these rendered
  // blocks back to their source AXNodes.
  std::vector<std::u16string> readability_text_blocks_;

  // Whether we should execute the mapping algorithm between the rendered text
  // and the AXtree that is used to populate the nodestore for a readability
  // distillation.
  bool should_map_rendered_text_to_tree_for_readability_ = false;

  // A mapping from a distilled Readability text block (by index) to its
  // corresponding AXTree segments.
  //
  // Structure:
  // The index of this vector matches the index of the block in
  // readability_text_blocks_.
  //   - Each entry is a vector of MappingSegments, because one distilled block
  //     might correspond to multiple underlying AXNodes (e.g., a paragraph
  //     containing a link).
  //   - If a block fails to map, the entry at that index will be an empty
  //     vector.
  std::vector<std::vector<MappingSegment>> text_to_ax_map_;

  // A contiguous string representation of all leaf text nodes in the original
  // page's AXTree, in reading order. This serves as the global "search space"
  // for the select text mapping algorithm.
  std::u16string global_ax_tree_text_;

  // A mapping that links character offsets in |global_ax_tree_text_| back to
  // their source AXNodes. Each segment stores the AXNodeID and its starting
  // position within the global string. This is used to translate mapping
  // results back into AXTree coordinates.
  std::vector<AXNodeSegment> flattened_ax_tree_nodes_;

  // Keeps track of which parts of the |global_ax_tree_text_| have already been
  // assigned to a full distilled block. Needed to ensure GapSubstringAlignment
  // doesn't map something to already mapped text in a gap (Ex: a shuffled
  // unique block not in the monotonic anchor list).
  // Each pair is [start, end)
  std::vector<std::pair<size_t, size_t>> occupied_ax_ranges_;

  // The minimum number of characters required for a substring to be considered
  // an anchor during GapSubstringAlignment mapping.
  static constexpr size_t kMinAnchorLength = 3;

  // The minimum number of characters required for a substring to be considered
  // valid during RelativeOrderAlignment mapping.
  static constexpr size_t kMinSequentialMatchLength = 5;

  // The distillation method that will be used for the next content update.
  DistillationMethod next_distillation_method_;

  // The distillation method that produced the content currently visible in the
  // UI.
  DistillationMethod current_content_distillation_method_;

  // Tracks whether the side panel distillation is derived from the main content
  // article (even for an empty distillation) or from a specific user
  // selection in the main panel.
  SidePanelDistillationMode side_panel_distillation_mode_ =
      SidePanelDistillationMode::kMainContent;

  std::map<ui::AXTreeID, ukm::SourceId> pending_ukm_sources_;

  // Possible child tree ids that could be used to distill content if the
  // root tree has no distillable content. This will only be used if
  // may_use_child_for_active_tree_ is true.
  std::set<ui::AXTreeID> child_tree_ids_;

  // If reading mode should attempt to use child trees to distill content. This
  // should only be true if the root tree has no distillable content.
  bool may_use_child_for_active_tree_ = false;

  // Whether the Readability-to-AXTree mapping algorithm is running or has
  // finished / not started.
  bool is_readability_mapping_in_progress_ = false;

  // Whether an early selection attempt has been logged for the current page.
  bool has_logged_early_selection_ = false;

  read_anything::mojom::ReadAnythingPresentationState
      active_presentation_state_ =
          read_anything::mojom::ReadAnythingPresentationState::kUndefined;
  read_anything::mojom::ReadAnythingDistillationState distillation_state_ =
      read_anything::mojom::ReadAnythingDistillationState::kNotAttempted;

  // List of observers of model state changes.
  base::ObserverList<ModelObserver, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<ReadAnythingAppModel> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_APP_MODEL_H_
