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
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace {

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kContentRoles[]{
    ax::mojom::Role::kHeading,
    ax::mojom::Role::kParagraph,
    ax::mojom::Role::kNote,
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
      on_ax_tree_distilled_callback_(on_ax_tree_distilled_callback) {
  // TODO(crbug.com/1450930): Use a global ukm recorder instance instead.
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
}

AXTreeDistiller::~AXTreeDistiller() = default;

void AXTreeDistiller::Distill(const ui::AXTree& tree,
                              const ui::AXTreeUpdate& snapshot,
                              const ukm::SourceId ukm_source_id) {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  base::TimeTicks start_time = base::TimeTicks::Now();
#endif
  // Try with the algorithm first.
  std::vector<ui::AXNodeID> content_node_ids;
  DistillViaAlgorithm(tree, ukm_source_id, &content_node_ids);

  // If Read Anything with Screen 2x is enabled and the main content extractor
  // is bound, kick off Screen 2x run, which distills the AXTree in the
  // utility process using ML.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled() &&
      main_content_extractor_.is_bound()) {
    DistillViaScreen2x(tree, snapshot, ukm_source_id, start_time,
                       &content_node_ids);
    return;
  }
#endif

  // Ensure we still callback if Screen2x is not available.
  on_ax_tree_distilled_callback_.Run(tree.GetAXTreeID(), content_node_ids);
}

void AXTreeDistiller::DistillViaAlgorithm(
    const ui::AXTree& tree,
    const ukm::SourceId ukm_source_id,
    std::vector<ui::AXNodeID>* content_node_ids) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::vector<const ui::AXNode*> content_root_nodes;
  GetContentRootNodes(tree.root(), &content_root_nodes);
  for (const ui::AXNode* content_root_node : content_root_nodes) {
    AddContentNodesToVector(content_root_node, content_node_ids);
  }
  RecordRulesMetrics(ukm_source_id, base::TimeTicks::Now() - start_time,
                     !content_node_ids->empty());
}

void AXTreeDistiller::RecordRulesMetrics(ukm::SourceId ukm_source_id,
                                         base::TimeDelta elapsed_time,
                                         bool success) {
  if (success) {
    base::UmaHistogramTimes(
        "Accessibility.ReadAnything.RulesDistillationTime.Success",
        elapsed_time);
    ukm::builders::Accessibility_ReadAnything(ukm_source_id)
        .SetRulesDistillationTime_Success(elapsed_time.InMilliseconds())
        .Record(ukm_recorder_.get());
  } else {
    base::UmaHistogramTimes(
        "Accessibility.ReadAnything.RulesDistillationTime.Failure",
        elapsed_time);
    ukm::builders::Accessibility_ReadAnything(ukm_source_id)
        .SetRulesDistillationTime_Failure(elapsed_time.InMilliseconds())
        .Record(ukm_recorder_.get());
  }
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXTreeDistiller::DistillViaScreen2x(
    const ui::AXTree& tree,
    const ui::AXTreeUpdate& snapshot,
    const ukm::SourceId ukm_source_id,
    base::TimeTicks start_time,
    std::vector<ui::AXNodeID>* content_node_ids_algorithm) {
  DCHECK(main_content_extractor_.is_bound());
  // Make a copy of |content_node_ids_algorithm| rather than sending a pointer.
  main_content_extractor_->ExtractMainContent(
      snapshot, ukm_source_id,
      base::BindOnce(&AXTreeDistiller::ProcessScreen2xResult,
                     weak_ptr_factory_.GetWeakPtr(), tree.GetAXTreeID(),
                     ukm_source_id, start_time, *content_node_ids_algorithm));
}

void AXTreeDistiller::ProcessScreen2xResult(
    const ui::AXTreeID& tree_id,
    const ukm::SourceId ukm_source_id,
    base::TimeTicks start_time,
    std::vector<ui::AXNodeID> content_node_ids_algorithm,
    const std::vector<ui::AXNodeID>& content_node_ids_screen2x) {
  // Merge the results from the algorithm and from screen2x.
  for (ui::AXNodeID content_node_id_screen2x : content_node_ids_screen2x) {
    if (!base::Contains(content_node_ids_algorithm, content_node_id_screen2x)) {
      content_node_ids_algorithm.push_back(content_node_id_screen2x);
    }
  }
  RecordMergedMetrics(ukm_source_id, base::TimeTicks::Now() - start_time,
                      !content_node_ids_algorithm.empty());
  on_ax_tree_distilled_callback_.Run(tree_id, content_node_ids_algorithm);

  // TODO(crbug.com/1266555): If no content nodes were identified, and
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

void AXTreeDistiller::RecordMergedMetrics(ukm::SourceId ukm_source_id,
                                          base::TimeDelta elapsed_time,
                                          bool success) {
  if (success) {
    base::UmaHistogramTimes(
        "Accessibility.ReadAnything.MergedDistillationTime.Success",
        elapsed_time);
    ukm::builders::Accessibility_ReadAnything(ukm_source_id)
        .SetMergedDistillationTime_Success(elapsed_time.InMilliseconds())
        .Record(ukm_recorder_.get());
  } else {
    base::UmaHistogramTimes(
        "Accessibility.ReadAnything.MergedDistillationTime.Failure",
        elapsed_time);
    ukm::builders::Accessibility_ReadAnything(ukm_source_id)
        .SetMergedDistillationTime_Failure(elapsed_time.InMilliseconds())
        .Record(ukm_recorder_.get());
  }
}
#endif
