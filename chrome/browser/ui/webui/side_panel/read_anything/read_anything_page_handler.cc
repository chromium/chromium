// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_coordinator.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

ReadAnythingPageHandler::ReadAnythingPageHandler(
    mojo::PendingRemote<read_anything::mojom::Page> page,
    mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  // Register |this| as a |ReadAnythingModel::Observer| with the coordinator
  // for the component. This will allow the IPC to update the front-end web ui.

  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;
  browser_view_ = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view_)
    return;
  browser_view_->side_panel_coordinator()
      ->read_anything_coordinator()
      ->AddObserver(this);
}

ReadAnythingPageHandler::~ReadAnythingPageHandler() {
  // Remove |this| from the observer list of |ReadAnythingModel|.
  if (browser_view_) {
    DCHECK(
        browser_view_->side_panel_coordinator()->read_anything_coordinator());
    browser_view_->side_panel_coordinator()
        ->read_anything_coordinator()
        ->RemoveObserver(this);
  }
}

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
  // Send the contents to the WebUI.
  page_->OnEssentialContent(std::move(content));
}

void ReadAnythingPageHandler::OnFontNameUpdated(
    const std::string& new_font_name) {
  page_->OnFontNameChange(new_font_name);
}
