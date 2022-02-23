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
    if (popped->GetRole() == ax::mojom::Role::kArticle)
      return popped;
    for (auto iter = popped->UnignoredChildrenBegin();
         iter != popped->UnignoredChildrenEnd(); ++iter) {
      queue.push(iter.get());
    }
  }

  return nullptr;
}

void AddTextNodesToVector(const ui::AXNode* node,
                          std::vector<std::string>* strings) {
  if (node->GetRole() == ax::mojom::Role::kStaticText) {
    std::string value;
    if (node->GetStringAttribute(ax::mojom::StringAttribute::kName, &value))
      strings->emplace_back(value);
    return;
  }

  for (const auto role : kRolesToSkip) {
    if (role == node->GetRole())
      return;
  }
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AddTextNodesToVector(iter.get(), strings);
  }
}

}  // namespace

ReaderModePageHandler::ReaderModePageHandler(
    mojo::PendingRemote<reader_mode::mojom::Page> page,
    mojo::PendingReceiver<reader_mode::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

ReaderModePageHandler::~ReaderModePageHandler() = default;

void ReaderModePageHandler::ShowUI() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Get page contents (via snapshot of a11y tree) for reader generation.
  // This will include subframe content for any subframes loaded at this point.
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&ReaderModePageHandler::CombineTextNodesAndMakeCallback,
                     weak_pointer_factory_.GetWeakPtr()),
      ui::AXMode::kWebContents,
      /* exclude_offscreen= */ false, kMaxNodes,
      /* timeout= */ {});
}

void ReaderModePageHandler::CombineTextNodesAndMakeCallback(
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

  page_->OnEssentialContent(std::move(text_node_contents));
}
