// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include <map>

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

// A class that holds state for the ReadAnythingAppController for the Read
// Anything WebUI app.
class ReadAnythingAppModel {
 public:
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

    // Map of text start and end indices of text to a specific AXNodeID.
    // The text for a given segment may span multiple AXNodes, such as
    // Node 1: This is a
    // Node 2: link
    // Node 3: in a separate node.
    // which is presented as a single segment when using sentence granularity.
    // However, when we get word callbacks, we get them in terms of the text
    // index across the entire segment of text, not by node. Therefore, this
    // mapping helps us better parse callbacks for different types of
    // granularity highlighting.
    // TODO(b/40927698): Investigate using this to replace
    // highlightedNodeToOffsetInParent in app.ts
    std::map<std::pair<int, int>, ui::AXNodeID> index_map;

    // The human readable text represented by this segment of node ids. This
    // is stored separately for easier retrieval for non-sentence granularity
    // highlighting.
    std::u16string text;
  };

  bool requires_distillation() { return requires_distillation_; }
  void set_requires_distillation(bool value) { requires_distillation_ = value; }
  bool requires_post_process_selection() {
    return requires_post_process_selection_;
  }
  void set_requires_post_process_selection(bool value) {
    requires_post_process_selection_ = value;
  }
  const ui::AXNodeID& image_to_update_node_id() {
    return image_to_update_node_id_;
  }
  void reset_image_to_update_node_id() {
    image_to_update_node_id_ = ui::kInvalidAXNodeID;
  }
  bool selection_from_action() { return selection_from_action_; }
  void set_selection_from_action(bool value) { selection_from_action_ = value; }

  const std::string& base_language_code() const { return base_language_code_; }

  void set_base_language_code(const std::string code) {
    base_language_code_ = code;
  }

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
  void set_font_name(const std::string& font) { font_name_ = font; }
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
  void SetVoice(const std::string& voice, const std::string& lang) {
    voices_.Set(lang, voice);
  }
  const base::Value::List& languages_enabled_in_pref() const {
    return languages_enabled_in_pref_;
  }

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

  bool page_finished_loading_for_data_collection() {
    return page_finished_loading_for_data_collection_;
  }
  void set_page_finished_loading_for_data_collection(bool value) {
    page_finished_loading_for_data_collection_ = value;
  }
  bool page_finished_loading() {
    return page_finished_loading_;
  }
  void set_page_finished_loading(bool value) {
    page_finished_loading_ = value;
  }
  bool speech_playing() { return speech_playing_; }
  void set_speech_playing(bool value) { speech_playing_ = value; }

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
  void set_active_tree_id(const ui::AXTreeID& active_tree_id) {
    active_tree_id_ = active_tree_id;
  }

  void SetDistillationInProgress(bool distillation) {
    distillation_in_progress_ = distillation;
  }

  const ukm::SourceId& ukm_source_id();
  void set_ukm_source_id(const ukm::SourceId ukm_source_id);
  int32_t num_selections();
  void set_num_selections(const int32_t& num_selections);

  void AddUrlInformationForTreeId(const ui::AXTreeID& tree_id);
  bool IsDocs() const;

  ui::AXNode* GetAXNode(const ui::AXNodeID& ax_node_id) const;
  bool IsNodeIgnoredForReadAnything(const ui::AXNodeID& ax_node_id) const;
  bool NodeIsContentNode(const ui::AXNodeID& ax_node_id) const;
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
      base::Value::List* languages_enabled_in_pref_,
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

  ui::AXSerializableTree* GetTreeFromId(const ui::AXTreeID& tree_id) const;
  void AddTree(const ui::AXTreeID& tree_id,
               std::unique_ptr<ui::AXSerializableTree> tree);

  bool ContainsTree(const ui::AXTreeID& tree_id) const;

  void UnserializePendingUpdates(const ui::AXTreeID& tree_id);

  void ClearPendingUpdates();

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  std::vector<ui::AXTreeUpdate>& updates,
                                  std::vector<ui::AXEvent>& events);

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

  std::string GetHtmlTag(const ui::AXNodeID& ax_node_id) const;
  std::string GetAltText(const ui::AXNodeID& ax_node_id) const;
  std::string GetImageDataUrl(const ui::AXNodeID& ax_node_id) const;

  // Returns the index of the next sentence of the given text, such that the
  // next sentence is equivalent to text.substr(0, <returned_index>).
  int GetNextSentence(const std::u16string& text);

  // Returns the index of the next word of the given text, such that the
  // next word is equivalent to text.substr(0, <returned_index>).
  int GetNextWord(const std::u16string& text);

  // Given a text index for the current granularity, return the AXNodeID for
  // that part of the text.
  // For example, if a current granularity segment has text:
  // "Hello darkness, my old friend."
  // Composed of nodes:
  // Node: {id: 113, text: "Hello darkness, "}
  // Node: {id: 207, text: "my old friend."}
  // Then GetNodeIdForCurrentSegmentIndex for index=0-16 will return "113"
  // and for index=17-29 will return "207"
  ui::AXNodeID GetNodeIdForCurrentSegmentIndex(int index) const;

  // Starting at the given index, return the length of the next word in the
  // current granularity.
  // e.g. if the current granularity is "I've come to talk with you again."...
  // A start index of "0" will return "3" to correspond to "I've"
  // And a start index of "10" will return "2" to correspond to "to."
  // This method is only forward-looking, so if an index is in the middle of a
  // current word, the remaining length for that word will be returned.
  // e.g. "1" will return "2" to "'ve"
  int GetNextWordHighlightLength(int start_index);

  // PDF handling.
  void set_is_pdf(bool is_pdf) { is_pdf_ = is_pdf; }
  bool is_pdf() const { return is_pdf_; }

  // Returns the next valid AXNodePosition.
  ui::AXNodePosition::AXPositionInstance
  GetNextValidPositionFromCurrentPosition(
      const ReadAnythingAppModel::ReadAloudCurrentGranularity&
          current_granularity);

  // Inits the AXPosition with a starting node.
  // TODO(crbug.com/40927698): We should be able to use AXPosition in a way
  // where this isn't needed.
  void InitAXPositionWithNode(const ui::AXNodeID& starting_node_id);

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
  int GetCurrentTextStartIndex(const ui::AXNodeID& node_id);

  // Returns the Read Aloud ending text index for a node. For example,
  // if the entire text of the node should be read by Read Aloud at a particular
  // moment, this will return the length of the node's text. Returns -1 if the
  // node isn't in the current segment.
  int GetCurrentTextEndIndex(const ui::AXNodeID& node_id);

  void IncrementMetric(const std::string& metric_name);

  // Log speech count events.
  void LogSpeechEventCounts();

 private:
  void EraseTree(const ui::AXTreeID& tree_id);

  void InsertDisplayNode(const ui::AXNodeID& node);
  void ResetSelection();
  void InsertSelectionNode(const ui::AXNodeID& node);
  void UpdateSelection();
  void ComputeSelectionNodeIds();
  bool NoCurrentSelection();
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

  ui::AXNode* GetParentForSelection(ui::AXNode* node);
  std::string GetHtmlTagForPDF(ui::AXNode* ax_node,
                               const std::string& html_tag) const;
  std::string GetHeadingHtmlTagForPDF(ui::AXNode* ax_node,
                                      const std::string& html_tag) const;

  // Uses the current AXNodePosition to return the next node that should be
  // spoken by Read Aloud.
  ui::AXNode* GetNodeFromCurrentPosition() const;

  void ResetReadAloudState();

  bool IsTextForReadAnything(const ui::AXNodeID& ax_node_id) const;

  bool ShouldSplitAtParagraph(
      const ui::AXNodePosition::AXPositionInstance& position,
      const ReadAloudCurrentGranularity& current_granularity) const;

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
      const ReadAnythingAppModel::ReadAloudCurrentGranularity&
          current_granularity,
      const ui::AXNodeID& id) const;

  // Helper method to get the correct anchor node from an AXPositionInstance
  // that should be used by Read Aloud. AXPosition can sometimes return
  // leaf nodes that don't actually correspond to the AXNodes we're using
  // in Reading Mode, so we need to get a parent node from the AXPosition's
  // returned anchor when this happens.
  ui::AXNode* GetAnchorNode(
      const ui::AXNodePosition::AXPositionInstance& position) const;

  bool IsOpeningPunctuation(char& c) const;

  bool IsValidAXPosition(
      const ui::AXNodePosition::AXPositionInstance& positin,
      const ReadAnythingAppModel::ReadAloudCurrentGranularity&
          current_granularity) const;

  // Returns true if both positions are non-null and equal.
  bool ArePositionsEqual(
      const ui::AXNodePosition::AXPositionInstance& position,
      const ui::AXNodePosition::AXPositionInstance& other) const;

  // Returns the index of the next granularity of the given text, such that the
  // next granularity is equivalent to text.substr(0, <returned_index>).
  int GetNextGranularity(const std::u16string& text,
                         ax::mojom::TextBoundary boundary);

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

  // Whether Read Aloud speech is currently playing or not.
  bool speech_playing_ = false;

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

  // The default language code, used as a fallback in case base_language_code_
  // is invalid. It's not guaranteed that default_language_code_ will always
  // be valid, but as it is tied to the browser language, it is likely more
  // stable than the base_language_code_, which may be changed on different
  // pages.
  std::string default_language_code_ = "en";

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
  base::Value::Dict voices_;
  base::Value::List languages_enabled_in_pref_;
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

  // For screen2x data collection, Chrome is launched from the CLI to open one
  // webpage. We record the result of the distill() call for this entire
  // webpage, so we only make the call once the webpage finished loading.
  bool page_finished_loading_for_data_collection_ = false;

  // Whether the webpage has finished loading or not.
  bool page_finished_loading_ = false;

  // Read Aloud state

  ui::AXNodePosition::AXPositionInstance ax_position_;

  // Our current index within processed_granularities_on_current_page_.
  size_t processed_granularity_index_ = 0;

  // The current text index within the given node.
  int current_text_index_ = 0;

  // TODO(crbug.com/40927698): Clear this when granularity changes.
  // TODO(crbug.com/40927698): Use this to assist in navigating forwards /
  // backwards.
  // Previously processed granularities on the current page.
  std::vector<ReadAnythingAppModel::ReadAloudCurrentGranularity>
      processed_granularities_on_current_page_;

  // Metrics for logging. Any metric that we want to track 0-counts of should
  // be initialized here.
  std::map<std::string, int64_t> metric_to_count_map_ = {
      {"Accessibility.ReadAnything.ReadAloudNextButtonSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPauseSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPlaySessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPreviousButtonSessionCount", 0},
  };
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
