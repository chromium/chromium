// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/ax_tree_distiller.h"

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace {

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kContentRoles[]{
    ax::mojom::Role::kHeading,
    ax::mojom::Role::kParagraph,
};

// TODO: Consider moving this to AXNodeProperties.
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

// Find all of the main and article nodes.
// TODO(crbug.com/1266555): Replace this with a call to
// OneShotAccessibilityTreeSearch.
void GetContentRootNodes(const ui::AXNode* root,
                         std::vector<const ui::AXNode*>* content_root_nodes) {
  if (!root) {
    return;
  }
  std::queue<const ui::AXNode*> queue;
  queue.push(root);
  while (!queue.empty()) {
    const ui::AXNode* node = queue.front();
    queue.pop();
    // If a main or article node is found, add it to the list of content root
    // nodes and continue. Do not explore children for nested article nodes.
    if (node->GetRole() == ax::mojom::Role::kMain ||
        node->GetRole() == ax::mojom::Role::kArticle) {
      content_root_nodes->push_back(node);
      continue;
    }
    for (auto iter = node->UnignoredChildrenBegin();
         iter != node->UnignoredChildrenEnd(); ++iter) {
      queue.push(iter.get());
    }
  }
}

// Recurse through the root node, searching for content nodes (any node whose
// role is in kContentRoles). Skip branches which begin with a node with role
// in kRolesToSkip. Once a content node is identified, add it to the vector
// |content_node_ids|, whose pointer is passed through the recursion.
void AddContentNodesToVector(const ui::AXNode* node,
                             std::vector<ui::AXNodeID>* content_node_ids) {
  if (base::Contains(kContentRoles, node->GetRole())) {
    content_node_ids->emplace_back(node->id());
    return;
  }
  if (base::Contains(kRolesToSkip, node->GetRole()))
    return;
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AddContentNodesToVector(iter.get(), content_node_ids);
  }
}

}  // namespace

AXTreeDistiller::AXTreeDistiller(
    content::RenderFrame* render_frame,
    OnAXTreeDistilledCallback on_ax_tree_distilled_callback)
    : render_frame_(render_frame),
      on_ax_tree_distilled_callback_(on_ax_tree_distilled_callback) {}

AXTreeDistiller::~AXTreeDistiller() = default;

void AXTreeDistiller::Distill(const ui::AXTree& tree,
                              const ui::AXTreeUpdate& snapshot,
                              const ukm::SourceId& ukm_source_id) {
  // If Read Anything with Screen 2x is enabled and the main content extractor
  // is bound, kick off Screen 2x run, which distills the AXTree in the utility
  // process using ML.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled() &&
      main_content_extractor_.is_bound()) {
    DistillViaScreen2x(tree, snapshot, ukm_source_id);
    return;
  }
#endif

  // Otherwise, distill the AXTree in process using the rules-based algorithm.
  DistillViaAlgorithm(tree);
}

void AXTreeDistiller::DistillViaAlgorithm(const ui::AXTree& tree) {
  std::vector<const ui::AXNode*> content_root_nodes;
  std::vector<ui::AXNodeID> content_node_ids;
  GetContentRootNodes(tree.root(), &content_root_nodes);
  for (const ui::AXNode* content_root_node : content_root_nodes) {
    AddContentNodesToVector(content_root_node, &content_node_ids);
  }
  on_ax_tree_distilled_callback_.Run(tree.GetAXTreeID(), content_node_ids);
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXTreeDistiller::DistillViaScreen2x(const ui::AXTree& tree,
                                         const ui::AXTreeUpdate& snapshot,
                                         const ukm::SourceId& ukm_source_id) {
  DCHECK(main_content_extractor_.is_bound());
  main_content_extractor_->ExtractMainContent(
      snapshot, ukm_source_id,
      base::BindOnce(&AXTreeDistiller::ProcessScreen2xResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::UnsafeDanglingUntriaged(
                         base::raw_ref<const ui::AXTree>(tree))));
}

void AXTreeDistiller::ProcessScreen2xResult(
    const ui::AXTree& tree,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // If content nodes were identified, run callback.
  if (!content_node_ids.empty()) {
    on_ax_tree_distilled_callback_.Run(tree.GetAXTreeID(), content_node_ids);
    return;
  }

  // Otherwise, try the rules-based approach.
  DistillViaAlgorithm(tree);

  // TODO(crbug.com/1266555): If still no content nodes were identified, and
  // there is a selection, try sending Screen2x a partial tree just containing
  // the selected nodes.
}

void AXTreeDistiller::ScreenAIServiceReady() {
  if (main_content_extractor_.is_bound()) {
    return;
  }
  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      main_content_extractor_.BindNewPipeAndPassReceiver());
  main_content_extractor_.set_disconnect_handler(
      base::BindOnce(&AXTreeDistiller::OnMainContentExtractorDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AXTreeDistiller::OnMainContentExtractorDisconnected() {
  on_ax_tree_distilled_callback_.Run(ui::AXTreeIDUnknown(),
                                     std::vector<ui::AXNodeID>());
}
#endif
