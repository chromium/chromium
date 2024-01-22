// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/renderer/accessibility/read_anything_app_model.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_tree_update_forward.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace ui {
class AXNode;
class AXSerializableTree;
class AXTree;
}  // namespace ui

class AXTreeDistiller;
class ReadAnythingAppControllerTest;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingAppController
//
//  A class that controls the Read Anything WebUI app. It serves two purposes:
//  1. Communicate with ReadAnythingUntrustedPageHandler (written in c++) via
//  mojom.
//  2. Communicate with ReadAnythingApp (written in ts) via gin bindings.
//  The ReadAnythingAppController unserializes the AXTreeUpdate and exposes
//  methods on it to the ts resource for accessing information about the AXTree.
//  This class is owned by the ChromeRenderFrameObserver and has the same
//  lifetime as the render frame.
//
//  This class is responsible for identifying the nodes to be displayed by the
//  webapp and providing attributes about them when queried. Nodes are selected
//  from the provided AXTreeUpdate and content nodes. There are two rendering
//  algorithms:
//  1. If the AXTreeUpdate has a selection, display a subtree containing all of
//     the nodes between the selection start and end.
//  2. If the AXTreeUpdate has no selection, display a subtree containing all of
//     the content nodes, their descendants, and their ancestors.
//
class ReadAnythingAppController
    : public gin::Wrappable<ReadAnythingAppController>,
      public read_anything::mojom::UntrustedPage {
 public:
  static gin::WrapperInfo kWrapperInfo;

  ReadAnythingAppController(const ReadAnythingAppController&) = delete;
  ReadAnythingAppController& operator=(const ReadAnythingAppController&) =
      delete;

  // Installs v8 context for Read Anything and adds chrome.readingMode binding
  // to page.
  static ReadAnythingAppController* Install(content::RenderFrame* render_frame);

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

    // Because GetNextText returns a vector of node ids to be used by
    // TypeScript also store the node ids as a vector for easier retrieval.
    std::vector<ui::AXNodeID> node_ids;
  };

 private:
  friend ReadAnythingAppControllerTest;

  explicit ReadAnythingAppController(content::RenderFrame* render_frame);
  ~ReadAnythingAppController() override;

  // gin::WrappableBase:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // read_anything::mojom::UntrustedPage:
  void AccessibilityEventReceived(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const std::vector<ui::AXEvent>& events) override;
  void OnActiveAXTreeIDChanged(const ui::AXTreeID& tree_id,
                               ukm::SourceId ukm_source_id,
                               const GURL& hostname,
                               bool force_update_state) override;
  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id) override;
  void OnThemeChanged(
      read_anything::mojom::ReadAnythingThemePtr new_theme) override;
  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      const std::string& font,
      double font_size,
      bool links_enabled,
      read_anything::mojom::Colors color,
      double speech_rate,
      base::Value::Dict voices,
      read_anything::mojom::HighlightGranularity granularity) override;
  void SetDefaultLanguageCode(const std::string& code) override;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void ScreenAIServiceReady() override;
#endif

  // Returns the next valid AXNodePosition.
  ui::AXNodePosition::AXPositionInstance
  GetNextValidPositionFromCurrentPosition(
      ReadAnythingAppController::ReadAloudCurrentGranularity
          current_granularity);

  // Uses the current AXNodePosition to return the next node that should be
  // spoken by Read Aloud.
  ui::AXNode* GetNodeFromCurrentPosition();

  // Returns true if the node was previously spoken or we expect to speak it
  // to be spoken once the current run of #GetNextText which called
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
      ReadAnythingAppController::ReadAloudCurrentGranularity
          current_granularity,
      ui::AXNodeID id);

  // gin templates:
  ui::AXNodeID RootId() const;
  ui::AXNodeID StartNodeId() const;
  int StartOffset() const;
  ui::AXNodeID EndNodeId() const;
  int EndOffset() const;
  SkColor BackgroundColor() const;
  std::string FontName() const;
  float FontSize() const;
  bool LinksEnabled() const;
  float SpeechRate() const;
  void OnFontSizeChanged(bool increase);
  void OnFontSizeReset();
  SkColor ForegroundColor() const;
  float LetterSpacing() const;
  float LineSpacing() const;
  int ColorTheme() const;
  int HighlightGranularity() const;
  int HighlightOn() const;
  int StandardLineSpacing() const;
  int LooseLineSpacing() const;
  int VeryLooseLineSpacing() const;
  int StandardLetterSpacing() const;
  int WideLetterSpacing() const;
  int VeryWideLetterSpacing() const;
  int DefaultTheme() const;
  int LightTheme() const;
  int DarkTheme() const;
  int YellowTheme() const;
  int BlueTheme() const;
  std::string GetStoredVoice(const std::string& lang) const;
  std::vector<ui::AXNodeID> GetChildren(ui::AXNodeID ax_node_id) const;
  std::string GetDataFontCss(ui::AXNodeID ax_node_id) const;
  std::string GetHtmlTag(ui::AXNodeID ax_node_id) const;
  std::string GetLanguage(ui::AXNodeID ax_node_id) const;
  std::string GetNameAttributeText(ui::AXNode* ax_node) const;
  std::string GetTextContent(ui::AXNodeID ax_node_id) const;
  std::string GetTextDirection(ui::AXNodeID ax_node_id) const;
  std::string GetUrl(ui::AXNodeID ax_node_id) const;
  bool ShouldBold(ui::AXNodeID ax_node_id) const;
  bool IsOverline(ui::AXNodeID ax_node_id) const;
  bool IsLeafNode(ui::AXNodeID ax_node_id) const;
  void OnConnected();
  void OnCopy() const;
  void OnScroll(bool on_selection) const;
  void OnLinkClicked(ui::AXNodeID ax_node_id) const;
  void OnSelectionChange(ui::AXNodeID anchor_node_id,
                         int anchor_offset,
                         ui::AXNodeID focus_node_id,
                         int focus_offset) const;
  void OnCollapseSelection() const;
  bool IsSelectable() const;
  bool IsWebUIToolbarEnabled() const;
  bool IsReadAloudEnabled() const;
  bool IsGoogleDocs() const;
  void OnStandardLineSpacing();
  void OnLooseLineSpacing();
  void OnVeryLooseLineSpacing();
  void OnStandardLetterSpacing();
  void OnWideLetterSpacing();
  void OnVeryWideLetterSpacing();
  void OnLightTheme();
  void OnDefaultTheme();
  void OnDarkTheme();
  void OnYellowTheme();
  void OnBlueTheme();
  void OnFontChange(const std::string& font);
  void OnSpeechRateChange(double rate);
  void OnVoiceChange(const std::string& voice, const std::string& lang);
  void TurnedHighlightOn();
  void TurnedHighlightOff();
  double GetLineSpacingValue(int line_spacing) const;
  double GetLetterSpacingValue(int letter_spacing) const;
  std::vector<std::string> GetSupportedFonts() const;

  std::string GetHtmlTagForPDF(ui::AXNode* ax_node, std::string html_tag) const;
  std::string GetHeadingHtmlTagForPDF(ui::AXNode* ax_node,
                                      std::string html_tag) const;
  std::string GetAriaLevel(ui::AXNode* ax_node) const;

  // The language code that should be used to determine which voices are
  // supported for speech.
  const std::string& GetLanguageCodeForSpeech() const;

  void Distill();
  void Draw();
  void DrawSelection();

  void ExecuteJavaScript(std::string script);

  void UnserializeUpdates(std::vector<ui::AXTreeUpdate> updates,
                          const ui::AXTreeID& tree_id);

  // Called when distillation has completed.
  void OnAXTreeDistilled(const ui::AXTreeID& tree_id,
                         const std::vector<ui::AXNodeID>& content_node_ids);

  void PostProcessSelection();

  // Signals that the side panel has finished loading and it's safe to show
  // the UI to avoid loading artifacts.
  void ShouldShowUI();

  // Inits the AXPosition with a starting node.
  // TODO(crbug.com/1474951): We should be able to use AXPosition in a way
  // where this isn't needed.
  void InitAXPositionWithNode(const ui::AXNodeID starting_node_id);

  // Returns a list of AXNodeIds representing the next nodes that should be
  // spoken and highlighted with Read Aloud. GetNextTextStartIndex and
  // GetNextTextEndIndex called with an AXNodeID return by GetNextText will
  // return the starting text and ending text indices for specific text that
  // should be referenced within the node.
  std::vector<ui::AXNodeID> GetNextText(int max_text_length);

  // Helper method for GetNextText.
  ReadAloudCurrentGranularity GetNextNodes(int max_text_length);

  // Returns a list of triples representing the previous nodes that should be
  // spoken and highlighted with Read Aloud. Each triple contains three numbers:
  // the AXNodeID, the starting text index, and the ending text index. This
  // list of triples is represented as a double array.
  std::vector<ui::AXNodeID> GetPreviousText(int max_text_length);

  // Returns the Read Aloud starting text index for a node. For example,
  // if the entire text of the node should be read by Read Aloud at a particular
  // moment, this will return 0. Returns -1 if the node isn't in the current
  // segment.
  int GetNextTextStartIndex(ui::AXNodeID node_id);

  // Returns the Read Aloud ending text index for a node. For example,
  // if the entire text of the node should be read by Read Aloud at a particular
  // moment, this will return the length of the node's text. Returns -1 if the
  // node isn't in the current segment.
  int GetNextTextEndIndex(ui::AXNodeID node_id);

  // Returns the index of the next sentence of the given text, such that the
  // next sentence is equivalent to text.substr(0, <returned_index>).
  // If the sentence exceeds the maximum text length, the sentence will be
  // cropped to the nearest word boundary that doesn't exceed the maximum
  // text length.
  int GetNextSentence(const std::u16string& text, int max_text_length);

  // SetContentForTesting, SetThemeForTesting, and SetLanguageForTesting are
  // used by ReadAnythingAppTest and thus need to be kept in
  // ReadAnythingAppController even though ReadAnythingAppControllerBrowserTest
  // is friended.
  // Snapshot_lite is a data structure which resembles an
  // AXTreeUpdate. E.g.:
  //   const axTree = {
  //     root_id: 1,
  //     nodes: [
  //       {
  //         id: 1,
  //         role: 'rootWebArea',
  //         child_ids: [2],
  //       },
  //       {
  //         id: 2,
  //         role: 'staticText',
  //         name: 'Some text.',
  //       },
  //     ],
  //   };
  void SetContentForTesting(v8::Local<v8::Value> v8_snapshot_lite,
                            std::vector<ui::AXNodeID> content_node_ids);
  void SetThemeForTesting(const std::string& font_name,
                          float font_size,
                          bool links_enabled,
                          SkColor foreground_color,
                          SkColor background_color,
                          int line_spacing,
                          int letter_spacing);
  void SetLanguageForTesting(const std::string& language_code);

  content::RenderFrame* GetRenderFrame();

  const blink::LocalFrameToken frame_token_;
  std::unique_ptr<AXTreeDistiller> distiller_;
  mojo::Remote<read_anything::mojom::UntrustedPageHandlerFactory>
      page_handler_factory_;
  mojo::Remote<read_anything::mojom::UntrustedPageHandler> page_handler_;
  mojo::Receiver<read_anything::mojom::UntrustedPage> receiver_{this};

  // TODO(crbug.com/1474951): Move Read Aloud state to Read Anything App Model.
  // Read Aloud state
  ui::AXNodePosition::AXPositionInstance ax_position_;
  // The current text index within the given node.
  int current_text_index_ = 0;

  // TODO(crbug.com/1474951): Clear this when granularity changes.
  // TODO(crbug.com/1474951): Use this to assist in navigating forwards /
  // backwards.
  // Previously processed granularities on the current page.
  std::vector<ReadAloudCurrentGranularity>
      processed_granularities_on_current_page_;

  // Our current index within processed_granularities_on_current_page_. If it is
  // equal to the size of the triples - 1, we're not navigating through
  // previously processed text.
  size_t processed_granularity_index_ = -1;

  // Model that holds state for this controller.
  ReadAnythingAppModel model_;

  base::WeakPtrFactory<ReadAnythingAppController> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
