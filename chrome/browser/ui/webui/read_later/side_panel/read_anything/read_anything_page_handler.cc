// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

ReadAnythingPageHandler::ReadAnythingPageHandler(
    mojo::PendingRemote<read_anything::mojom::Page> page,
    mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

ReadAnythingPageHandler::~ReadAnythingPageHandler() = default;

void ReadAnythingPageHandler::ShowUI() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;

  // Read Anything just runs on the main frame and does not run on embedded
  // content.
  content::RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  if (!render_frame_host)
    return;

  // Request a distilled AXTree for the main frame.
  render_frame_host->RequestDistilledAXTree(
      base::BindOnce(&ReadAnythingPageHandler::OnAXTreeDistilled,
                     weak_pointer_factory_.GetWeakPtr()));
}

void ReadAnythingPageHandler::OnAXTreeDistilled(
    const ui::AXTreeUpdate& snapshot,
    const std::vector<ui::AXNodeID>& text_node_ids) {
  ui::AXTree tree;
  bool success = tree.Unserialize(snapshot);
  if (!success)
    return;
  std::vector<std::string> text_node_contents;
  text_node_contents.resize(text_node_ids.size());
  for (size_t i = 0; i < text_node_ids.size(); ++i) {
    ui::AXNode* node = tree.GetFromId(text_node_ids[i]);
    if (!node)
      continue;
    std::string value;
    if (node->GetStringAttribute(ax::mojom::StringAttribute::kName, &value))
      text_node_contents[i] = value;
  }
  page_->OnEssentialContent(std::move(text_node_contents));
}
