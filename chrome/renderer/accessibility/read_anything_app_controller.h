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

#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/renderer/accessibility/read_anything_app_model.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/accessibility/ax_tree_update_forward.h"

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
//  1. Communicate with ReadAnythingPageHandler (written in c++) via mojom.
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
      public read_anything::mojom::Page,
      public ui::AXTreeObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  ReadAnythingAppController(const ReadAnythingAppController&) = delete;
  ReadAnythingAppController& operator=(const ReadAnythingAppController&) =
      delete;

  // Installs v8 context for Read Anything and adds chrome.readAnything binding
  // to page.
  static ReadAnythingAppController* Install(content::RenderFrame* render_frame);

 private:
  friend ReadAnythingAppControllerTest;

  explicit ReadAnythingAppController(content::RenderFrame* render_frame);
  ~ReadAnythingAppController() override;

  // gin::WrappableBase:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // read_anything::mojom::Page:
  void AccessibilityEventReceived(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const std::vector<ui::AXEvent>& events) override;
  void OnActiveAXTreeIDChanged(const ui::AXTreeID& tree_id,
                               ukm::SourceId ukm_source_id) override;
  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id) override;
  void OnThemeChanged(
      read_anything::mojom::ReadAnythingThemePtr new_theme) override;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void ScreenAIServiceReady() override;
#endif

  // ui::AXTreeObserver:
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;
  // TODO(crbug.com/1266555): Implement OnNodeWillBeDeleted to capture the
  // deletion of child trees.

  // gin templates:
  ui::AXNodeID RootId() const;
  ui::AXNodeID StartNodeId() const;
  int StartOffset() const;
  ui::AXNodeID EndNodeId() const;
  int EndOffset() const;
  SkColor BackgroundColor() const;
  std::string FontName() const;
  float FontSize() const;
  SkColor ForegroundColor() const;
  float LetterSpacing() const;
  float LineSpacing() const;
  std::vector<ui::AXNodeID> GetChildren(ui::AXNodeID ax_node_id) const;
  std::string GetHtmlTag(ui::AXNodeID ax_node_id) const;
  std::string GetLanguage(ui::AXNodeID ax_node_id) const;
  std::string GetTextContent(ui::AXNodeID ax_node_id) const;
  std::string GetTextDirection(ui::AXNodeID ax_node_id) const;
  std::string GetUrl(ui::AXNodeID ax_node_id) const;
  bool ShouldBold(ui::AXNodeID ax_node_id) const;
  bool IsOverline(ui::AXNodeID ax_node_id) const;
  void OnConnected();
  void OnLinkClicked(ui::AXNodeID ax_node_id) const;
  void OnSelectionChange(ui::AXNodeID anchor_node_id,
                         int anchor_offset,
                         ui::AXNodeID focus_node_id,
                         int focus_offset) const;

  void Distill();
  void Draw();

  void UnserializeUpdates(std::vector<ui::AXTreeUpdate> updates,
                          const ui::AXTreeID& tree_id);

  // Called when distillation has completed.
  void OnAXTreeDistilled(const ui::AXTreeID& tree_id,
                         const std::vector<ui::AXNodeID>& content_node_ids);

  // Helper functions for the rendering algorithm. Post-process the AXTree and
  // cache values before sending an `updateContent` notification to the Read
  // Anything app.ts. These functions:
  // 1. Save state related to selection (start_node_, end_node_, start_offset_,
  //    end_offset_).
  // 2. Save the display_node_ids_, which is a set of all nodes to be displayed
  //    in Read Anything app.ts.
  void PostProcessAXTreeWithSelection();
  void PostProcessDistillableAXTree();

  // The following methods are used for testing ReadAnythingAppTest.
  // Snapshot_lite is a data structure which resembles an AXTreeUpdate. E.g.:
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
  AXTreeDistiller* SetDistillerForTesting(
      std::unique_ptr<AXTreeDistiller> distiller);
  void SetPageHandlerForTesting(
      mojo::PendingRemote<read_anything::mojom::PageHandler> page_handler);

  ui::AXNode* GetAXNode(ui::AXNodeID ax_node_id) const;
  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) const;

  bool NodeIsContentNode(ui::AXNodeID ax_node_id) const;

  content::RenderFrame* render_frame_;
  std::unique_ptr<AXTreeDistiller> distiller_;
  mojo::Remote<read_anything::mojom::PageHandlerFactory> page_handler_factory_;
  mojo::Remote<read_anything::mojom::PageHandler> page_handler_;
  mojo::Receiver<read_anything::mojom::Page> receiver_{this};

  // State:

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

  // Model that holds state for this controller.
  ReadAnythingAppModel model_;
  base::WeakPtrFactory<ReadAnythingAppController> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
