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
#include "content/public/browser/page.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "url/gurl.h"

namespace {

using ax::mojom::IntAttribute;
using ax::mojom::Role;
using ax::mojom::StringAttribute;
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
        ax_node->GetIntAttribute(IntAttribute::kHierarchicalLevel);
  } else if (ui::IsLink(ax_node->GetRole())) {
    content_node->type = ContentType::kLink;
    content_node->url =
        GURL(ax_node->GetStringAttribute(StringAttribute::kUrl));
  } else if (ax_node->GetRole() == Role::kParagraph) {
    content_node->type = ContentType::kParagraph;
  } else if (ax_node->GetRole() == Role::kStaticText) {
    content_node->type = ContentType::kStaticText;
    content_node->text = ax_node->GetTextContentUTF8();
  } else {
    return nullptr;
  }

  // Set ContentNode.children.
  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    ContentNodePtr child_content_node = GetFromAXNode(it.get());
    if (child_content_node)
      content_node->children.push_back(std::move(child_content_node));
  }

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
  WebContentsObserver::Observe(nullptr);
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

void ReadAnythingController::PrimaryPageChanged(content::Page& page) {
  DistillAXTree();
}

void ReadAnythingController::DistillAXTree() {
  DCHECK(browser_);
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  WebContentsObserver::Observe(web_contents);

  // Read Anything just runs on the main frame and does not run on embedded
  // content.
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
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
