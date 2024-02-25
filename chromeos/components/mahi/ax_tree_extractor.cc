// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/ax_tree_extractor.h"

#include <queue>
#include <string>

#include "base/containers/contains.h"
#include "base/i18n/break_iterator.h"
#include "base/strings/string_util.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace mahi {

namespace {

using ::base::i18n::BreakIterator;

static const ax::mojom::Role kContentRoles[]{
    ax::mojom::Role::kHeading,
    ax::mojom::Role::kParagraph,
    ax::mojom::Role::kNote,
};

static const ax::mojom::Role kRolesToSkip[]{
    ax::mojom::Role::kAudio,
    ax::mojom::Role::kBanner,
    ax::mojom::Role::kButton,
    ax::mojom::Role::kComplementary,
    ax::mojom::Role::kContentInfo,
    ax::mojom::Role::kFooter,
    ax::mojom::Role::kFooterAsNonLandmark,
    ax::mojom::Role::kImage,
    ax::mojom::Role::kLabelText,
    ax::mojom::Role::kNavigation,
};

// Recurse through the root node, searching for content nodes (any node whose
// role is in kContentRoles). Skip branches which begin with a node with role
// in kRolesToSkip. Once a content node is identified, add it to the vector
// `content_node_ids`, whose pointer is passed through the recursion. For nodes
// that does not fall into either role list, we further dive into its child
// nodes until either eligible node if found or we have reached the leave of the
// tree.
void AddContentNodesToVector(const ui::AXNode* node,
                             std::vector<ui::AXNodeID>* content_node_ids) {
  if (base::Contains(kContentRoles, node->GetRole())) {
    content_node_ids->emplace_back(node->id());
    return;
  }
  if (base::Contains(kRolesToSkip, node->GetRole())) {
    return;
  }
  // The node's role not in either kContentRoles or kRolesToSkip. Check its
  // child nodes.
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AddContentNodesToVector(iter.get(), content_node_ids);
  }
}

// Get contents from the a11y tree based on the `content_node_ids`.
std::u16string GetContents(const ui::AXNode* root,
                           const std::vector<ui::AXNodeID>& content_node_ids) {
  std::u16string contents = std::u16string();
  if (!root || content_node_ids.empty()) {
    return contents;
  }

  std::queue<const ui::AXNode*> queue;
  queue.push(root);
  while (!queue.empty()) {
    const ui::AXNode* node = queue.front();
    queue.pop();
    // If a content node is found, add its content to the result and continue.
    if (base::Contains(content_node_ids, node->id())) {
      if (!contents.empty()) {
        contents.append(u"\n\n");
      }
      contents.append(node->GetTextContentUTF16());
      continue;
    }
    for (auto iter = node->UnignoredChildrenBegin();
         iter != node->UnignoredChildrenEnd(); ++iter) {
      queue.push(iter.get());
    }
  }
  return contents;
}

// Get word count from contents.
int GetContentsWordCount(std::u16string& contents) {
  int word_count = 0;
  BreakIterator break_iter(contents, BreakIterator::BREAK_WORD);
  if (!break_iter.Init()) {
    return word_count;
  }

  while (break_iter.Advance()) {
    if (break_iter.IsWord()) {
      ++word_count;
    }
  }
  return word_count;
}

}  // namespace

AXTreeExtractor::AXTreeExtractor() = default;

AXTreeExtractor::~AXTreeExtractor() = default;

void AXTreeExtractor::OnScreen2xReady(
    mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
        screen2x_content_extractor) {
  // Drop the callback if the extractor is already bound.
  if (screen2x_main_content_extractor_.is_bound()) {
    return;
  }
  screen2x_main_content_extractor_.Bind(std::move(screen2x_content_extractor));
}

void AXTreeExtractor::ExtractContent(
    mojom::ExtractionRequestPtr extraction_request,
    ExtractContentCallback callback) {
  // Deserializes the snapshot.
  ui::AXTree tree(extraction_request->snapshot);

  std::vector<ui::AXNodeID> content_node_ids;
  if (extraction_request->extraction_methods->use_algorithm) {
    DistillViaAlgorithm(tree, &content_node_ids);
  }

  if (extraction_request->extraction_methods->use_screen2x &&
      screen2x_main_content_extractor_.is_bound() &&
      screen2x_main_content_extractor_.is_connected()) {
    // TODO(b/318565573): send to screen2x
    return;
  }

  // If screen2x is not available when receiving a request, report the error
  // status. Don't early return here as rule based algorithm may still work.
  mojom::ResponseStatus error_status = mojom::ResponseStatus::kSuccess;
  if (extraction_request->extraction_methods->use_screen2x) {
    error_status = mojom::ResponseStatus::kScreen2xNotAvailable;
  }
  OnDistilledForContentExtraction(tree, std::move(callback), error_status,
                                  content_node_ids);
}

void AXTreeExtractor::GetContentSize(
    mojom::ExtractionRequestPtr content_size_request,
    GetContentSizeCallback callback) {
  // Deserializes the snapshot.
  ui::AXTree tree(content_size_request->snapshot);

  std::vector<ui::AXNodeID> content_node_ids;
  if (content_size_request->extraction_methods->use_algorithm) {
    DistillViaAlgorithm(tree, &content_node_ids);
  }

  if (content_size_request->extraction_methods->use_screen2x &&
      screen2x_main_content_extractor_.is_bound() &&
      screen2x_main_content_extractor_.is_connected()) {
    // TODO(b/318565573): send to screen2x
    return;
  }

  // If screen2x is not available when receiving a request, report the error
  // status. Don't early return here as rule based algorithm may still work.
  mojom::ResponseStatus error_status = mojom::ResponseStatus::kSuccess;
  if (content_size_request->extraction_methods->use_screen2x) {
    error_status = mojom::ResponseStatus::kScreen2xNotAvailable;
  }
  OnDistilledForContentSize(tree, std::move(callback), error_status,
                            content_node_ids);
}

void AXTreeExtractor::DistillViaAlgorithm(
    const ui::AXTree& tree,
    std::vector<ui::AXNodeID>* content_node_ids) {
  AddContentNodesToVector(tree.root(), content_node_ids);
}

void AXTreeExtractor::OnDistilledForContentExtraction(
    const ui::AXTree& tree,
    ExtractContentCallback callback,
    mojom::ResponseStatus error_status,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  mojom::ExtractionResponsePtr extraction_response =
      mojom::ExtractionResponse::New();
  extraction_response->contents = GetContents(tree.root(), content_node_ids);
  extraction_response->status = error_status;

  std::move(callback).Run(std::move(extraction_response));
}

void AXTreeExtractor::OnDistilledForContentSize(
    const ui::AXTree& tree,
    GetContentSizeCallback callback,
    mojom::ResponseStatus error_status,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  mojom::ContentSizeResponsePtr content_size_response =
      mojom::ContentSizeResponse::New();
  std::u16string contents = GetContents(tree.root(), content_node_ids);
  content_size_response->word_count = GetContentsWordCount(contents);
  content_size_response->status = error_status;

  std::move(callback).Run(std::move(content_size_response));
}

}  // namespace mahi
