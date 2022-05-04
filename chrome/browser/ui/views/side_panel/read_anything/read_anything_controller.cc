// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"

namespace {

using read_anything::mojom::ContentNode;
using read_anything::mojom::ContentNodePtr;
using read_anything::mojom::ContentType;

ContentNodePtr GetFromAXNode(ui::AXNode* ax_node) {
  auto content_node = ContentNode::New();

  // Set ContentNode.type. If ax_node role doesn't map to a ContentType, return
  // nullptr.
  if (ui::IsHeading(ax_node->GetRole())) {
    content_node->type = ContentType::kHeading;
    content_node->heading_level =
        ax_node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
  } else if (ax_node->GetRole() == ax::mojom::Role::kParagraph) {
    content_node->type = ContentType::kParagraph;
  } else {
    return nullptr;
  }

  // Set ContentNode.text. If ax_node doesn't contain any text, return nullptr.
  if (!ax_node->GetTextContentLengthUTF8())
    return nullptr;
  content_node->text = ax_node->GetTextContentUTF8();

  return content_node;
}

}  // namespace

ReadAnythingController::ReadAnythingController(ReadAnythingModel* model,
                                               Browser* browser)
    : model_(model), browser_(browser) {
  DCHECK(browser_);
  if (browser_->tab_strip_model())
    browser_->tab_strip_model()->AddObserver(this);
}

ReadAnythingController::~ReadAnythingController() {
  DCHECK(browser_);
  if (browser_->tab_strip_model())
    browser_->tab_strip_model()->RemoveObserver(this);
}

void ReadAnythingController::OnFontChoiceChanged(int new_choice) {
  model_->SetSelectedFontIndex(new_choice);
}

void ReadAnythingController::OnUIReady() {
  DistillAXTree();
}

void ReadAnythingController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed())
    return;
  DistillAXTree();
}

void ReadAnythingController::DistillAXTree() {
  DCHECK(browser_);
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;

  // Read Anything just runs on the main frame and does not run on embedded
  // content.
  content::RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  if (!render_frame_host)
    return;

  // Request a distilled AXTree for the main frame.
  render_frame_host->RequestDistilledAXTree(
      base::BindOnce(&ReadAnythingController::OnAXTreeDistilled,
                     weak_pointer_factory_.GetWeakPtr()));
}

void ReadAnythingController::OnAXTreeDistilled(
    const ui::AXTreeUpdate& snapshot,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // Unserialize the snapshot.
  ui::AXTree tree;
  bool success = tree.Unserialize(snapshot);
  if (!success)
    return;

  std::vector<ContentNodePtr> content_nodes;
  for (auto ax_node_id : content_node_ids) {
    ui::AXNode* ax_node = tree.GetFromId(ax_node_id);
    if (!ax_node)
      continue;
    auto content_node = GetFromAXNode(ax_node);
    if (!content_node)
      continue;
    content_nodes.push_back(std::move(content_node));
  }

  // Update the content in the model.
  model_->SetContent(std::move(content_nodes));
}
