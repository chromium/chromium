// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/ax_tree_extractor.h"

#include <memory>
#include <queue>
#include <string>

#include "base/containers/contains.h"
#include "base/i18n/break_iterator.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"

namespace mahi {

namespace {

using ::base::i18n::BreakIterator;

// Time after which an idle connection to Screen AI service is disconnected.
constexpr base::TimeDelta kScreenAIIdleDisconnectDelay = base::Minutes(5);

static const ax::mojom::Role kContentRoles[]{
    ax::mojom::Role::kHeading,
    ax::mojom::Role::kParagraph,
    ax::mojom::Role::kNote,
};

static const ax::mojom::Role kRolesToSkip[]{
    ax::mojom::Role::kAudio,       ax::mojom::Role::kBanner,
    ax::mojom::Role::kButton,      ax::mojom::Role::kComplementary,
    ax::mojom::Role::kContentInfo, ax::mojom::Role::kFooter,
    ax::mojom::Role::kImage,       ax::mojom::Role::kLabelText,
    ax::mojom::Role::kNavigation,  ax::mojom::Role::kSectionFooter,
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
void GetContents(const ui::AXNode* root,
                 const std::vector<ui::AXNodeID>& content_node_ids,
                 std::u16string* contents) {
  if (!root || content_node_ids.empty()) {
    return;
  }

  // If a content node is found, add its content to the result and early return.
  if (base::Contains(content_node_ids, root->id())) {
    if (!contents->empty()) {
      contents->append(u"\n\n");
    }
    contents->append(root->GetTextContentUTF16());
    return;
  }
  // Use dfs search to ensure the contents is the same order as users see them
  // in the page.
  // TODO(chenjih): Revisit this if ax tree can be super deep. But this should
  // be quite rare.
  for (auto iter = root->UnignoredChildrenBegin();
       iter != root->UnignoredChildrenEnd(); ++iter) {
    GetContents(iter.get(), content_node_ids, contents);
  }
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
  // Etablish a connection with screen AI service is not already made and set
  // it to reset if it stays idle for `kScreenAIIdleDisconnectDelay` to release
  // resources.
  screen2x_main_content_extractor_.Bind(std::move(screen2x_content_extractor));
  screen2x_main_content_extractor_.reset_on_disconnect();
  screen2x_main_content_extractor_.reset_on_idle_timeout(
      kScreenAIIdleDisconnectDelay);
}

void AXTreeExtractor::ExtractContent(
    mojom::ExtractionRequestPtr extraction_request,
    ExtractContentCallback callback) {
  if (extraction_request->snapshot.has_value()) {
    ExtractContentFromSnapshot(std::move(extraction_request),
                               std::move(callback));
  } else {
    if (!extraction_request->updates.has_value()) {
      mojo::ReportBadMessage("No AXTree snapshot or updates were detected.");
      std::move(callback).Run(nullptr);
      return;
    }
    ExtractContentFromAXTreeUpdates(std::move(extraction_request),
                                    std::move(callback));
  }
}

void AXTreeExtractor::GetContentSize(
    mojom::ExtractionRequestPtr content_size_request,
    GetContentSizeCallback callback) {
  // Deserializes the snapshot.
  std::unique_ptr<ui::AXTree> tree =
      std::make_unique<ui::AXTree>(content_size_request->snapshot.value());

  std::vector<ui::AXNodeID> content_node_ids;
  if (content_size_request->extraction_methods->use_algorithm) {
    DistillViaAlgorithm(tree.get(), &content_node_ids);
  }

  mojom::ResponseStatus error_status = mojom::ResponseStatus::kSuccess;
  if (content_size_request->extraction_methods->use_screen2x &&
      screen2x_main_content_extractor_.is_bound() &&
      screen2x_main_content_extractor_.is_connected()) {
    OnAxTreeDistilledCallback on_ax_tree_distilled_callback =
        base::BindOnce(&AXTreeExtractor::OnDistilledForContentSize,
                       weak_ptr_factory_.GetWeakPtr(), std::move(tree),
                       std::move(callback), error_status);

    screen2x_main_content_extractor_->ExtractMainContent(
        content_size_request->snapshot.value(),
        content_size_request->ukm_source_id.value(),
        base::BindOnce(&AXTreeExtractor::OnGetScreen2xResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(content_node_ids),
                       std::move(on_ax_tree_distilled_callback)));
    return;
  }

  // If screen2x is not available when receiving a request, report the error
  // status. Don't early return here as rule based algorithm may still work.
  if (content_size_request->extraction_methods->use_screen2x) {
    error_status = mojom::ResponseStatus::kScreen2xNotAvailable;
  }
  OnDistilledForContentSize(std::move(tree), std::move(callback), error_status,
                            content_node_ids);
}

void AXTreeExtractor::ExtractContentFromSnapshot(
    mojom::ExtractionRequestPtr extraction_request,
    ExtractContentCallback callback) {
  if (!extraction_request->snapshot.has_value()) {
    mojo::ReportBadMessage("No AXTree snapshot were detected.");
    std::move(callback).Run(nullptr);
    return;
  }

  // Deserializes the snapshot.
  std::unique_ptr<ui::AXTree> tree =
      std::make_unique<ui::AXTree>(extraction_request->snapshot.value());

  std::vector<ui::AXNodeID> content_node_ids;
  if (extraction_request->extraction_methods->use_algorithm) {
    DistillViaAlgorithm(tree.get(), &content_node_ids);
  }

  mojom::ResponseStatus error_status = mojom::ResponseStatus::kSuccess;
  if (extraction_request->extraction_methods->use_screen2x &&
      screen2x_main_content_extractor_.is_bound() &&
      screen2x_main_content_extractor_.is_connected()) {
    auto on_ax_tree_distilled_callback =
        base::BindOnce(&AXTreeExtractor::OnDistilledForContentExtraction,
                       weak_ptr_factory_.GetWeakPtr(), std::move(tree),
                       std::move(callback), error_status);

    screen2x_main_content_extractor_->ExtractMainContent(
        extraction_request->snapshot.value(),
        extraction_request->ukm_source_id.value(),
        base::BindOnce(&AXTreeExtractor::OnGetScreen2xResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(content_node_ids),
                       std::move(on_ax_tree_distilled_callback)));
    return;
  }

  // If screen2x is not available when receiving a request, report the error
  // status. Don't early return here as rule based algorithm may still work.
  if (extraction_request->extraction_methods->use_screen2x) {
    error_status = mojom::ResponseStatus::kScreen2xNotAvailable;
  }
  OnDistilledForContentExtraction(std::move(tree), std::move(callback),
                                  error_status, content_node_ids);
}

// TODO(b:333803190): consider merging this with ExtractContentFromSnapshot if
// possible.
void AXTreeExtractor::ExtractContentFromAXTreeUpdates(
    mojom::ExtractionRequestPtr extraction_request,
    ExtractContentCallback callback) {
  if (!extraction_request->updates.has_value()) {
    mojo::ReportBadMessage("No AXTree updates were detected.");
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<ui::AXTree> tree = std::make_unique<ui::AXTree>();
  // Unserialize the updates.
  for (const ui::AXTreeUpdate& update : extraction_request->updates.value()) {
    tree->Unserialize(update);
  }

  std::vector<ui::AXNodeID> content_node_ids;
  if (extraction_request->extraction_methods->use_algorithm) {
    DistillViaAlgorithm(tree.get(), &content_node_ids);
  }

  // TODO(b:333803190): Figure out how to call screen2x using the tree.

  OnDistilledForContentExtraction(std::move(tree), std::move(callback),
                                  mojom::ResponseStatus::kScreen2xNotAvailable,
                                  content_node_ids);
}

void AXTreeExtractor::DistillViaAlgorithm(
    const ui::AXTree* tree,
    std::vector<ui::AXNodeID>* content_node_ids) {
  AddContentNodesToVector(tree->root(), content_node_ids);
}

void AXTreeExtractor::OnGetScreen2xResult(
    std::vector<ui::AXNodeID> content_node_ids_algorithm,
    OnAxTreeDistilledCallback on_ax_tree_distilled_callback,
    const std::vector<ui::AXNodeID>& content_node_ids_screen2x) {
  // Merges the results of algorithm and screen2x.
  for (ui::AXNodeID content_node_id_screen2x : content_node_ids_screen2x) {
    if (!base::Contains(content_node_ids_algorithm, content_node_id_screen2x)) {
      content_node_ids_algorithm.push_back(content_node_id_screen2x);
    }
  }
  std::move(on_ax_tree_distilled_callback).Run(content_node_ids_algorithm);
}

void AXTreeExtractor::OnDistilledForContentExtraction(
    std::unique_ptr<ui::AXTree> tree,
    ExtractContentCallback callback,
    mojom::ResponseStatus error_status,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  mojom::ExtractionResponsePtr extraction_response =
      mojom::ExtractionResponse::New();
  std::u16string contents;
  GetContents(tree->root(), content_node_ids, &contents);
  extraction_response->contents = std::move(contents);
  extraction_response->status = error_status;

  std::move(callback).Run(std::move(extraction_response));
}

void AXTreeExtractor::OnDistilledForContentSize(
    std::unique_ptr<ui::AXTree> tree,
    GetContentSizeCallback callback,
    mojom::ResponseStatus error_status,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  mojom::ContentSizeResponsePtr content_size_response =
      mojom::ContentSizeResponse::New();
  std::u16string contents;
  GetContents(tree->root(), content_node_ids, &contents);
  content_size_response->word_count = GetContentsWordCount(contents);
  content_size_response->status = error_status;

  std::move(callback).Run(std::move(content_size_response));
}

}  // namespace mahi
