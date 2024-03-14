// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
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

  // A current segment of text that will be consumed by Read Aloud.
  struct ReadAloudTextSegment {
    // The AXNodeID associated with this particular text segment.
    ui::AXNodeID id;

    // The starting index for the text with the node of the given id.
    int text_start;

    // The ending index for the text with the node of the given id.
    int text_end;
  };

  // A representation of multiple ReadAloudTextSegments that are processed
  // by Read Aloud at a single moment. For example, when using sentence
  // granularity, the list of ReadAloudTextSegments in a
  // ReadAloudCurrentGranularity will include all ReadAloudTextSegments
  // necessary to represent a single sentence.
  struct ReadAloudCurrentGranularity {
    ReadAloudCurrentGranularity();
    ReadAloudCurrentGranularity(const ReadAloudCurrentGranularity& other);
    ~ReadAloudCurrentGranularity();

    // Adds a segment to the current granularity.
    void AddSegment(ReadAloudTextSegment segment) {
      segments[segment.id] = segment;
      node_ids.push_back(segment.id);
    }

    // All of the ReadAloudTextSegments in the current granularity.
    std::map<ui::AXNodeID, ReadAloudTextSegment> segments;

    // Because GetCurrentText returns a vector of node ids to be used by
    // TypeScript also store the node ids as a vector for easier retrieval.
    std::vector<ui::AXNodeID> node_ids;
  };

  bool requires_distillation() { return requires_distillation_; }
  void set_requires_distillation(bool value) { requires_distillation_ = value; }
  bool requires_post_process_selection() {
    return requires_post_process_selection_;
  }
  void set_requires_post_process_selection(bool value) {
    requires_post_process_selection_ = value;
  }
  ui::AXNodeID image_to_update_node_id() { return image_to_update_node_id_; }
  void reset_image_to_update_node_id() {
    image_to_update_node_id_ = ui::kInvalidAXNodeID;
  }
  bool selection_from_action() { return selection_from_action_; }
  void set_selection_from_action(bool value) { selection_from_action_ = value; }

  const std::string& default_language_code() const {
    return default_language_code_;
  }

  void set_default_language_code(const std::string code) {
    default_language_code_ = code;
  }

  std::vector<std::string> GetSupportedFonts() const;

  // TODO(b/1266555): Ensure there is proper test coverage for all methods.
  // Theme
  const std::string& font_name() const { return font_name_; }
  float font_size() const { return font_size_; }
  bool links_enabled() const { return links_enabled_; }
  float letter_spacing() const { return letter_spacing_; }
  float line_spacing() const { return line_spacing_; }
  int color_theme() const { return color_theme_; }
  int highlight_granularity() const { return highlight_granularity_; }
  const SkColor& foreground_color() const { return foreground_color_; }
  const SkColor& background_color() const { return background_color_; }
  float speech_rate() const { return speech_rate_; }
  const base::Value::Dict& voices() const { return voices_; }

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

  bool page_finished_loading_for_data_collection() const {
    return page_finished_loading_for_data_collection_;
  }

  const ukm::SourceId& active_ukm_source_id() const {
    return active_ukm_source_id_;
  }

  const std::vector<ui::AXNodeID>& content_node_ids() const {
    return content_node_ids_;
  }
  const std::set<ui::AXNodeID>& display_node_ids() const {
    return display_node_ids_;
  }
  const std::set<ui::AXNodeID>& selection_node_ids() const {
    return selection_node_ids_;
  }

  const ui::AXTreeID active_tree_id() const { return active_tree_id_; }
  void set_active_tree_id(const ui::AXTreeID active_tree_id) {
    active_tree_id_ = active_tree_id;
  }

  void SetDistillationInProgress(bool distillation) {
    distillation_in_progress_ = distillation;
  }
  void SetActiveTreeSelectable(bool active_tree_selectable) {
    active_tree_selectable_ = active_tree_selectable;
  }
  void SetActiveUkmSourceId(ukm::SourceId source_id);

  ui::AXNode* GetAXNode(ui::AXNodeID ax_node_id) const;
  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) const;
  bool NodeIsContentNode(ui::AXNodeID ax_node_id) const;
  void OnThemeChanged(read_anything::mojom::ReadAnythingThemePtr new_theme);
  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      const std::string& font,
      double font_size,
      bool links_enabled,
      read_anything::mojom::Colors color,
      double speech_rate,
      base::Value::Dict* voices,
      read_anything::mojom::HighlightGranularity granularity);
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

  ui::AXSerializableTree* GetTreeFromId(ui::AXTreeID tree_id) const;
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

  std::map<ui::AXTreeID, std::unique_ptr<ui::AXTreeManager>>*
  GetTreesForTesting();

  void EraseTreeForTesting(ui::AXTreeID tree_id);

  double GetLineSpacingValue(
      read_anything::mojom::LineSpacing line_spacing) const;
  double GetLetterSpacingValue(
      read_anything::mojom::LetterSpacing letter_spacing) const;

  void IncreaseTextSize();
  void DecreaseTextSize();
  void ResetTextSize();
  void ToggleLinksEnabled();

  std::string GetHtmlTag(ui::AXNodeID ax_node_id) const;
  std::string GetAltText(ui::AXNodeID ax_node_id) const;
  std::string GetImageDataUrl(ui::AXNodeID ax_node_id) const;

  // Returns the index of the next sentence of the given text, such that the
  // next sentence is equivalent to text.substr(0, <returned_index>).
  int GetNextSentence(const std::u16string& text);

  // PDF handling.
  void set_is_pdf(bool is_pdf) { is_pdf_ = is_pdf; }
  bool is_pdf() const { return is_pdf_; }

  // Google Docs need special handling.
  void set_is_google_docs(bool is_google_docs) { is_docs_ = is_google_docs; }
  bool is_docs() const { return is_docs_; }

  // Returns the next valid AXNodePosition.
  ui::AXNodePosition::AXPositionInstance
  GetNextValidPositionFromCurrentPosition(
      ReadAnythingAppModel::ReadAloudCurrentGranularity& current_granularity);

  // Inits the AXPosition with a starting node.
  // TODO(crbug.com/1474951): We should be able to use AXPosition in a way
  // where this isn't needed.
  void InitAXPositionWithNode(const ui::AXNodeID starting_node_id);

  // Returns a list of AXNodeIds representing the next nodes that should be
  // spoken and highlighted with Read Aloud.
  // This defaults to returning the first granularity until
  // MovePositionTo<Next,Previous>Granularity() moves the position.
  // If the the current processed_granularity_index_ has not been calculated
  // yet, GetNextNodes() is called which updates the AXPosition.
  // GetCurrentTextStartIndex and GetCurrentTextEndIndex called with an AXNodeID
  // return by GetCurrentText will return the starting text and ending text
  // indices for specific text that should be referenced within the node.
  std::vector<ui::AXNodeID> GetCurrentText();

  // Increments the processed_granularity_index_, updating ReadAloud's state of
  // the current granularity to refer to the next granularity. The current
  // behavior allows the client to increment past the end of the page's content.
  void MovePositionToNextGranularity();

  // Decrements the processed_granularity_index_,updating ReadAloud's state of
  // the current granularity to refer to the previous granularity. Cannot be
  // decremented less than 0.
  void MovePositionToPreviousGranularity();

  // Helper method for GetCurrentText.
  ReadAloudCurrentGranularity GetNextNodes();

  // Returns the Read Aloud starting text index for a node. For example,
  // if the entire text of the node should be read by Read Aloud at a particular
  // moment, this will return 0. Returns -1 if the node isn't in the current
  // segment.
  int GetCurrentTextStartIndex(ui::AXNodeID node_id);

  // Returns the Read Aloud ending text index for a node. For example,
  // if the entire text of the node should be read by Read Aloud at a particular
  // moment, this will return the length of the node's text. Returns -1 if the
  // node isn't in the current segment.
  int GetCurrentTextEndIndex(ui::AXNodeID node_id);

 private:
  void EraseTree(ui::AXTreeID tree_id);

  void InsertDisplayNode(ui::AXNodeID node);
  void ResetSelection();
  void InsertSelectionNode(ui::AXNodeID node);
  void UpdateSelection();
  void ComputeSelectionNodeIds();
  bool NoCurrentSelection();
  bool SelectionInsideDisplayNodes();
  bool ContentNodesOnlyContainHeadings();

  void AddPendingUpdates(const ui::AXTreeID tree_id,
                         const std::vector<ui::AXTreeUpdate>& updates);

  void UnserializeUpdates(const std::vector<ui::AXTreeUpdate>& updates,
                          const ui::AXTreeID& tree_id);

  const std::vector<ui::AXTreeUpdate>& GetOrCreatePendingUpdateAt(
      ui::AXTreeID tree_id);

  void ProcessNonGeneratedEvents(const std::vector<ui::AXEvent>& events);

  // The tree size arguments are used to determine if distillation of a PDF is
  // necessary.
  void ProcessGeneratedEvents(const ui::AXEventGenerator& event_generator,
                              size_t prev_tree_size,
                              size_t tree_size);

  ui::AXNode* GetParentForSelection(ui::AXNode* node);
  std::string GetHtmlTagForPDF(ui::AXNode* ax_node, std::string html_tag) const;
  std::string GetHeadingHtmlTagForPDF(ui::AXNode* ax_node,
                                      std::string html_tag) const;
  std::string GetAriaLevel(ui::AXNode* ax_node) const;

  // Uses the current AXNodePosition to return the next node that should be
  // spoken by Read Aloud.
  ui::AXNode* GetNodeFromCurrentPosition() const;

  void ResetReadAloudState();

  bool IsTextForReadAnything(ui::AXNodeID ax_node_id) const;

  bool ShouldSplitAtParagraph(
      ui::AXNodePosition::AXPositionInstance& position,
      ReadAloudCurrentGranularity& current_granularity) const;

  // Returns true if the node was previously spoken or we expect to speak it
  // to be spoken once the current run of #GetCurrentText which called
  // #NodeBeenOrWillBeSpoken finishes executing. Because AXPosition
  // sometimes returns leaf nodes, we sometimes need to use the parent of a
  // node returned by AXPosition instead of the node itself. Because of this,
  // we need to double-check that the node has not been used or currently
  // in use.
  // Example:
  // parent node: id=5
  //    child node: id=6
  //    child node: id =7
  // node: id = 10
  // Where AXPosition will return nodes in order of 6, 7, 10, but Reading Mode
  // process them as 5, 10. Without checking for previously spoken nodes,
  // id 5 will be spoken twice.
  bool NodeBeenOrWillBeSpoken(
      ReadAnythingAppModel::ReadAloudCurrentGranularity& current_granularity,
      ui::AXNodeID id) const;

  // Helper method to get the correct anchor node from an AXPositionInstance
  // that should be used by Read Aloud. AXPosition can sometimes return
  // leaf nodes that don't actually correspond to the AXNodes we're using
  // in Reading Mode, so we need to get a parent node from the AXPosition's
  // returned anchor when this happens.
  ui::AXNode* GetAnchorNode(
      ui::AXNodePosition::AXPositionInstance& position) const;

  bool IsOpeningPunctuation(char& c) const;

  bool IsValidAXPosition(ui::AXNodePosition::AXPositionInstance& positin,
                         ReadAnythingAppModel::ReadAloudCurrentGranularity&
                             current_granularity) const;

  // State.
  // Store AXTrees of web contents in the browser's tab strip as AXTreeManagers.
  std::map<ui::AXTreeID, std::unique_ptr<ui::AXTreeManager>> tree_managers_;

  // The AXTreeID of the currently active web contents. For PDFs, this will
  // always be the AXTreeID of the main web contents (not the PDF iframe or its
  // child).
  ui::AXTreeID active_tree_id_ = ui::AXTreeIDUnknown();

  // The UKM source ID of the main frame of the active web contents, whose
  // AXTree has ID active_tree_id_. This is used for metrics collection.
  ukm::SourceId active_ukm_source_id_ = ukm::kInvalidSourceId;

  // Certain websites (e.g. Docs and PDFs) are not distillable with selection.
  bool active_tree_selectable_ = true;

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

  std::string default_language_code_ = "en-US";

  // Theme information.
  std::string font_name_ = string_constants::kReadAnythingPlaceholderFontName;
  float font_size_ = kReadAnythingDefaultFontScale;
  bool links_enabled_ = kReadAnythingDefaultLinksEnabled;
  float letter_spacing_ =
      (int)read_anything::mojom::LetterSpacing::kDefaultValue;
  float line_spacing_ = (int)read_anything::mojom::LineSpacing::kDefaultValue;
  SkColor background_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  SkColor foreground_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  int color_theme_ = (int)read_anything::mojom::Colors::kDefaultValue;
  float speech_rate_ = kReadAnythingDefaultSpeechRate;
  base::Value::Dict voices_ = base::Value::Dict();
  int highlight_granularity_ =
      (int)read_anything::mojom::HighlightGranularity::kDefaultValue;

  // Selection information.
  bool has_selection_ = false;
  ui::AXNodeID start_node_id_ = ui::kInvalidAXNodeID;
  ui::AXNodeID end_node_id_ = ui::kInvalidAXNodeID;
  int32_t start_offset_ = -1;
  int32_t end_offset_ = -1;
  bool requires_distillation_ = false;
  bool requires_post_process_selection_ = false;
  ui::AXNodeID image_to_update_node_id_ = ui::kInvalidAXNodeID;
  bool selection_from_action_ = false;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // Used to keep track of how many selections were made for the
  // active_ukm_source_id_. Only recorded during the select-to-distill flow
  // (when the empty state page is shown).
  int32_t num_selections_ = 0;

  // For screen2x data collection, Chrome is launched from the CLI to open one
  // webpage. We record the result of the distill() call for this entire
  // webpage, so we only make the call once the webpage finished loading.
  bool page_finished_loading_for_data_collection_ = false;

  // Google Docs are different from regular webpages. We want to distill content
  // from the annotated canvas elements, not the main tree.
  bool is_docs_ = false;

  // Read Aloud state

  ui::AXNodePosition::AXPositionInstance ax_position_;

  // Our current index within processed_granularities_on_current_page_.
  size_t processed_granularity_index_ = 0;

  // The current text index within the given node.
  int current_text_index_ = 0;

  // TODO(crbug.com/1474951): Clear this when granularity changes.
  // TODO(crbug.com/1474951): Use this to assist in navigating forwards /
  // backwards.
  // Previously processed granularities on the current page.
  std::vector<ReadAnythingAppModel::ReadAloudCurrentGranularity>
      processed_granularities_on_current_page_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
