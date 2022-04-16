// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

ReadAnythingController::ReadAnythingController(ReadAnythingModel* model,
                                               Browser* browser)
    : model_(model), browser_(browser) {}

void ReadAnythingController::OnFontChoiceChanged(int new_choice) {
  model_->SetSelectedFontIndex(new_choice);
}

void ReadAnythingController::OnUIShown() {
  if (!browser_)
    return;

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

  std::vector<std::string> content;
  // Iterate through all content node ids.
  for (auto node_id : content_node_ids) {
    // Find the node in the tree which has this node id.
    ui::AXNode* node = tree.GetFromId(node_id);
    if (!node)
      continue;

    // Get the complete text content for the node and add it to a vector of
    // contents.
    // TODO: Handle links.
    if (node->GetTextContentLengthUTF8())
      content.push_back(node->GetTextContentUTF8());
  }

  // Update the content in the model.
  model_->SetContent(content);
}

ReadAnythingController::~ReadAnythingController() = default;
