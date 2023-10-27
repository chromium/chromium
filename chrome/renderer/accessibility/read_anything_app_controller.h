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
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_id_forward.h"
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
                               const GURL& hostname) override;
  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id) override;
  void OnThemeChanged(
      read_anything::mojom::ReadAnythingThemePtr new_theme) override;
  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      const std::string& font,
      double font_size,
      read_anything::mojom::Colors color,
      double speech_rate,
      read_anything::mojom::HighlightGranularity granularity) override;
  void SetDefaultLanguageCode(const std::string& code) override;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void ScreenAIServiceReady() override;
#endif

  // gin templates:
  ui::AXNodeID RootId() const;
  ui::AXNodeID StartNodeId() const;
  int StartOffset() const;
  ui::AXNodeID EndNodeId() const;
  int EndOffset() const;
  SkColor BackgroundColor() const;
  std::string FontName() const;
  float FontSize() const;
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
  std::vector<ui::AXNodeID> GetChildren(ui::AXNodeID ax_node_id) const;
  std::string GetHtmlTag(ui::AXNodeID ax_node_id) const;
  std::string GetLanguage(ui::AXNodeID ax_node_id) const;
  std::string GetTextContent(ui::AXNodeID ax_node_id) const;
  std::string GetTextDirection(ui::AXNodeID ax_node_id) const;
  std::string GetUrl(ui::AXNodeID ax_node_id) const;
  bool ShouldBold(ui::AXNodeID ax_node_id) const;
  bool IsOverline(ui::AXNodeID ax_node_id) const;
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
  void TurnedHighlightOn();
  void TurnedHighlightOff();
  double GetLineSpacingValue(int line_spacing) const;
  double GetLetterSpacingValue(int letter_spacing) const;
  std::vector<std::string> GetSupportedFonts() const;

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

  // Returns the index of the next sentence of the given text, such that the
  // next sentence is equivalent to text.substr(0, <returned_index>).
  // If the sentence exceeds the maximum text length, the sentence will be
  // cropped to the nearest word boundary that doesn't exceed the maximum
  // text length.
  int GetNextSentence(const std::u16string& text, int maxTextLength);

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
                          SkColor foreground_color,
                          SkColor background_color,
                          int line_spacing,
                          int letter_spacing);
  void SetLanguageForTesting(const std::string& language_code);

  const int render_frame_id_;
  std::unique_ptr<AXTreeDistiller> distiller_;
  mojo::Remote<read_anything::mojom::UntrustedPageHandlerFactory>
      page_handler_factory_;
  mojo::Remote<read_anything::mojom::UntrustedPageHandler> page_handler_;
  mojo::Receiver<read_anything::mojom::UntrustedPage> receiver_{this};

  // Model that holds state for this controller.
  ReadAnythingAppModel model_;

  base::WeakPtrFactory<ReadAnythingAppController> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
