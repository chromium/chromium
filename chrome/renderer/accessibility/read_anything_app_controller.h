// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/common/accessibility/read_anything.mojom.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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
  void OnFontNameChange(const std::string& new_font_name) override;
  void OnFontSizeChanged(const float new_font_size) override;

  // gin templates:
  std::vector<ui::AXNodeID> ContentNodeIds();
  std::string FontName();
  float FontSize();
  std::vector<ui::AXNodeID> GetChildren(ui::AXNodeID ax_node_id);
  uint32_t GetHeadingLevel(ui::AXNodeID ax_node_id);
  std::string GetTextContent(ui::AXNodeID ax_node_id);
  std::string GetUrl(ui::AXNodeID ax_node_id);
  bool IsHeading(ui::AXNodeID ax_node_id);
  bool IsLink(ui::AXNodeID ax_node_id);
  bool IsParagraph(ui::AXNodeID ax_node_id);
  bool IsStaticText(ui::AXNodeID ax_node_id);
  void OnConnected();

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
  void SetFontNameForTesting(std::string new_font_name);

  ui::AXNode* GetAXNode(ui::AXNodeID ax_node_id);

  content::RenderFrame* render_frame_;
  mojo::Remote<read_anything::mojom::PageHandlerFactory> page_handler_factory_;
  mojo::Remote<read_anything::mojom::PageHandler> page_handler_;
  mojo::Receiver<read_anything::mojom::Page> receiver_{this};

  // State
  std::unique_ptr<ui::AXTree> tree_;
  std::vector<ui::AXNodeID> content_node_ids_;
  std::string font_name_;
  float font_size_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_CONTROLLER_H_
