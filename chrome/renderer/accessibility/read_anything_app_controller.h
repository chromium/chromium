// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "chrome/common/accessibility/read_anything.mojom.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace ui {
class AXNode;
class AXTree;
}  // namespace ui

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
      public read_anything::mojom::Page {
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
  void OnAXTreeDistilled(
      const ui::AXTreeUpdate& snapshot,
      const std::vector<ui::AXNodeID>& content_node_ids) override;
  void OnThemeChanged(
      read_anything::mojom::ReadAnythingThemePtr new_theme) override;

  // gin templates:
  ui::AXNodeID RootId() const;
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
  void OnConnected();
  void OnLinkClicked(ui::AXNodeID ax_node_id) const;

  // Helper functions for the rendering algorithm. Post-process the AXTree and
  // cache values before sending an `updateContent` notification to the Read
  // Anything app.ts. These functions:
  // 1. Save state related to selection (start_node_, end_node_, start_offset_,
  //    end_offset_).
  // 2. Save the display_node_ids_, which is a set of all nodes to be displayed
  //    in Read Anything app.ts.
  void PostProcessAXTreeWithSelection(const ui::AXTreeData& tree_data);
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
  double GetLetterSpacingValue(
      read_anything::mojom::Spacing letter_spacing) const;
  double GetLineSpacingValue(read_anything::mojom::Spacing line_spacing) const;

  ui::AXNode* GetAXNode(ui::AXNodeID ax_node_id) const;

  bool NodeIsContentNode(ui::AXNodeID ax_node_id) const;

  content::RenderFrame* render_frame_;
  mojo::Remote<read_anything::mojom::PageHandlerFactory> page_handler_factory_;
  mojo::Remote<read_anything::mojom::PageHandler> page_handler_;
  mojo::Receiver<read_anything::mojom::Page> receiver_{this};

  // State
  std::unique_ptr<ui::AXTree> tree_;
  std::vector<ui::AXNodeID> content_node_ids_;
  std::set<ui::AXNodeID> display_node_ids_;
  bool has_selection_ = false;
  ui::AXNode* start_node_ = nullptr;
  ui::AXNode* end_node_ = nullptr;
  int32_t start_offset_ = -1;
  int32_t end_offset_ = -1;

  SkColor background_color_;
  std::string font_name_;
  float font_size_;
  SkColor foreground_color_;
  float letter_spacing_;
  float line_spacing_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
