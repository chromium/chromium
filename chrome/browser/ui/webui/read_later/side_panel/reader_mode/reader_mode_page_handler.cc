// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/side_panel/reader_mode/reader_mode_page_handler.h"

#include <algorithm>
#include <queue>
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

namespace {

static const ax::mojom::Role kRolesToSkip[]{
    ax::mojom::Role::kAudio,
    ax::mojom::Role::kBanner,
    ax::mojom::Role::kButton,
    ax::mojom::Role::kComplementary,
    ax::mojom::Role::kContentInfo,
    ax::mojom::Role::kFooter,
    ax::mojom::Role::kFooterAsNonLandmark,
    ax::mojom::Role::kHeader,
    ax::mojom::Role::kHeaderAsNonLandmark,
    ax::mojom::Role::kImage,
    ax::mojom::Role::kLabelText,
    ax::mojom::Role::kNavigation,
};
static constexpr int kMaxNodes = 5000;

// TODO(crbug.com/1266555): Replace this with a call to
// OneShotAccessibilityTreeSearch.
const ui::AXNode* GetArticleNode(const ui::AXNode* node) {
  std::queue<const ui::AXNode*> queue;
  queue.push(node);

  while (!queue.empty()) {
    const ui::AXNode* popped = queue.front();
    queue.pop();
    if (popped->data().role == ax::mojom::Role::kArticle)
      return popped;
    for (size_t i = 0; i < popped->GetUnignoredChildCount(); ++i)
      queue.push(popped->GetUnignoredChildAtIndex(i));
  }

  return nullptr;
}

void AddTextNodesToVector(const ui::AXNode* node,
                          std::vector<std::string>* strings) {
  const ui::AXNodeData& node_data = node->data();

  if (node_data.role == ax::mojom::Role::kStaticText) {
    if (node_data.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
      strings->emplace_back(
          node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
    }
    return;
  }

  for (const auto role : kRolesToSkip) {
    if (role == node_data.role)
      return;
  }
  for (size_t i = 0; i < node->GetUnignoredChildCount(); ++i)
    AddTextNodesToVector(node->GetUnignoredChildAtIndex(i), strings);
}

}  // namespace

ReaderModePageHandler::ReaderModePageHandler(
    mojo::PendingReceiver<reader_mode::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

ReaderModePageHandler::~ReaderModePageHandler() = default;

void ReaderModePageHandler::ShowReaderMode(ShowReaderModeCallback callback) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Get page contents (via snapshot of a11y tree) for reader generation.
  // This will include subframe content for any subframes loaded at this point.
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&ReaderModePageHandler::CombineTextNodesAndMakeCallback,
                     weak_pointer_factory_.GetWeakPtr(), std::move(callback)),
      ui::AXMode::kWebContents,
      /* exclude_offscreen= */ false, kMaxNodes,
      /* timeout= */ {});
}

void ReaderModePageHandler::CombineTextNodesAndMakeCallback(
    ShowReaderModeCallback callback,
    const ui::AXTreeUpdate& update) {
  ui::AXTree tree;
  bool success = tree.Unserialize(update);
  if (!success)
    return;

  // If this page has an article node, only combine text from that node.
  const ui::AXNode* reader_root = GetArticleNode(tree.root());
  if (!reader_root) {
    reader_root = tree.root();
  }

  std::vector<std::string> text_node_contents;
  text_node_contents.reserve(update.nodes.size());
  AddTextNodesToVector(reader_root, &text_node_contents);

  std::move(callback).Run(text_node_contents);
}
