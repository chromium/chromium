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
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_computed_node_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"

namespace {

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kContentRoles[]{
    ax::mojom::Role::kHeading, ax::mojom::Role::kParagraph,
    ax::mojom::Role::kNote, ax::mojom::Role::kImage,
    ax::mojom::Role::kFigcaption};

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kRolesToSkip[]{
    ax::mojom::Role::kAudio,         ax::mojom::Role::kBanner,
    ax::mojom::Role::kButton,        ax::mojom::Role::kComplementary,
    ax::mojom::Role::kContentInfo,   ax::mojom::Role::kFooter,
    ax::mojom::Role::kLabelText,     ax::mojom::Role::kNavigation,
    ax::mojom::Role::kSectionFooter,
};

// Find all of the main and article nodes. Also, include unignored heading nodes
// which lie outside of the main and article node.
// TODO(crbug.com/40802192): Replace this with a call to
// OneShotAccessibilityTreeSearch.
void GetContentRootNodes(const ui::AXTree& tree,
                         std::vector<const ui::AXNode*>* content_root_nodes) {
  const ui::AXNode* root = tree.root();
  if (!root) {
    return;
  }

  std::queue<const ui::AXNode*> queue;
  queue.push(root);
  bool has_main_or_heading = false;
  while (!queue.empty()) {
    const ui::AXNode* node = queue.front();
    queue.pop();
    // If a main or article node is found, add it to the list of content root
    // nodes and continue. Do not explore children for nested article nodes.
    if (node->GetRole() == ax::mojom::Role::kMain ||
        node->GetRole() == ax::mojom::Role::kArticle) {
      content_root_nodes->push_back(node);
      has_main_or_heading = true;
      continue;
    }
    // If a heading node is found, add it to the list of content root nodes,
    // too. It may be removed later if the tree doesn't contain a main or
    // article node. Do not add it if it is offscreen.
    if (node->GetRole() == ax::mojom::Role::kHeading) {
      bool offscreen = false;
      tree.GetTreeBounds(node, &offscreen);
      if (offscreen) {
        continue;
      }
      content_root_nodes->push_back(node);
      continue;
    }

    // Add all nodes that can be expanded. Collapsed nodes will be removed
    // later.
    if (node->HasHtmlAttribute("aria-expanded")) {
      content_root_nodes->push_back(node);
      continue;
    }

    if (node->HasState(ax::mojom::State::kRichlyEditable)) {
      content_root_nodes->push_back(node);
      continue;
    }

    // Search through all children.
    for (auto iter = node->UnignoredChildrenBegin();
         iter != node->UnignoredChildrenEnd(); ++iter) {
      queue.push(iter.get());
    }
  }
  if (!has_main_or_heading) {
    content_root_nodes->clear();
  }
}

// Recurse through the root node, searching for content nodes (any node whose
// role is in kContentRoles). Skip branches which begin with a node with role
// in kRolesToSkip. Once a content node is identified, add it to the vector
// |content_node_ids|, whose pointer is passed through the recursion.
void AddContentNodesToVector(const ui::AXNode* node,
                             std::vector<ui::AXNodeID>* content_node_ids) {
  const auto& role = node->GetRole();
  if (base::Contains(kContentRoles, role)) {
    // TODO(crbug.com/40922922): Remove when flag is no longer necessary. Skip
    // these roles if the flag is not enabled.
    if (!features::IsReadAnythingImagesViaAlgorithmEnabled() &&
        (role == ax::mojom::Role::kFigcaption ||
         role == ax::mojom::Role::kImage)) {
      return;
    }
    content_node_ids->emplace_back(node->id());
    return;
  }

  if (node->HasState(ax::mojom::State::kRichlyEditable) &&
      node->id() == node->tree()->data().focus_id) {
    content_node_ids->push_back(node->id());
    return;
  }

  const std::string& aria_expanded_state =
      node->GetHtmlAttribute("aria-expanded");
  if (aria_expanded_state == "true") {
    content_node_ids->push_back(node->id());
    return;
  }

  if (base::Contains(kRolesToSkip, node->GetRole())) {
    return;
  }
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AddContentNodesToVector(iter.get(), content_node_ids);
  }
}

}  // namespace

AXTreeDistiller::AXTreeDistiller(
    content::RenderFrame* render_frame,
    OnAXTreeDistilledCallback on_ax_tree_distilled_callback)
    : content::RenderFrameObserver(render_frame),
      on_ax_tree_distilled_callback_(on_ax_tree_distilled_callback) {
  // TODO(crbug.com/40915547): Use a global ukm recorder instance instead.
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
}

AXTreeDistiller::~AXTreeDistiller() = default;

void AXTreeDistiller::Distill(const ui::AXTree& tree,
                              const ui::AXTreeUpdate& snapshot,
                              const ukm::SourceId ukm_source_id) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  std::vector<ui::AXNodeID> content_node_ids;
  if (features::IsReadAnythingWithAlgorithmEnabled()) {
    // Try with the algorithm first.
    DistillViaAlgorithm(tree, ukm_source_id, &content_node_ids);
  }

  // If Read Anything with Screen 2x is enabled and Screen AI service is ready,
  // kick off Screen 2x run, which distills the AXTree in the utility process
  // using ML.
  if (features::IsReadAnythingWithScreen2xEnabled() &&
      screen_ai_service_ready_) {
    DistillViaScreen2x(tree, snapshot, ukm_source_id, start_time,
                       &content_node_ids);
    return;
  }

  // Ensure we still callback if Screen2x is not available.
  on_ax_tree_distilled_callback_.Run(tree.GetAXTreeID(), content_node_ids);
}

void AXTreeDistiller::DistillViaAlgorithm(
    const ui::AXTree& tree,
    const ukm::SourceId ukm_source_id,
    std::vector<ui::AXNodeID>* content_node_ids) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::vector<const ui::AXNode*> content_root_nodes;
  GetContentRootNodes(tree, &content_root_nodes);
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

void AXTreeDistiller::DistillViaScreen2x(
    const ui::AXTree& tree,
    const ui::AXTreeUpdate& snapshot,
    const ukm::SourceId ukm_source_id,
    base::TimeTicks start_time,
    std::vector<ui::AXNodeID>* content_node_ids_algorithm) {
  CHECK(screen_ai_service_ready_);

  // Establish connection to ScreenAI service if it's not already made.
  if (!main_content_extractor_.is_bound()) {
    render_frame()->GetBrowserInterfaceBroker().GetInterface(
        main_content_extractor_.BindNewPipeAndPassReceiver());
    main_content_extractor_.set_disconnect_handler(
        base::BindOnce(&AXTreeDistiller::OnMainContentExtractorDisconnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

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

  // TODO(crbug.com/40802192): If no content nodes were identified, and
  // there is a selection, try sending Screen2x a partial tree just containing
  // the selected nodes.
}

void AXTreeDistiller::ScreenAIServiceReady() {
  screen_ai_service_ready_ = true;
}

void AXTreeDistiller::OnMainContentExtractorDisconnected() {
  main_content_extractor_.reset();
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
