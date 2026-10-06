// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/reading_mode_metrics/reading_mode_metrics_service.h"

#include <vector>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "chrome/services/reading_mode_metrics/reading_mode_metrics.rs.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace reading_mode {

namespace {

struct ExtractState {
  raw_ptr<ui::AXNode> node;
  bool inside_code;
  bool in_link;
  bool in_list;
};

// Extracts structural properties, formatting text fragments, and link density
// metrics from the AXTree.
void ExtractOriginalStructure(ui::AXNode* root,
                              reading_mode::OriginalStructure& structure,
                              bool root_inside_code,
                              bool root_in_link) {
  if (!root) {
    return;
  }

  structure.original_total_text_len = 0;
  structure.original_link_text_len = 0;

  std::vector<ExtractState> stack;
  // Pre-allocate space based on max AXTree size (5000 nodes) to avoid heap
  // resizing.
  stack.reserve(5000);
  stack.push_back({root, root_inside_code, root_in_link, /*in_list=*/false});

  while (!stack.empty()) {
    ExtractState current = stack.back();
    stack.pop_back();

    ui::AXNode* node = current.node.get();
    bool inside_code = current.inside_code;
    bool in_link = current.in_link;
    bool in_list = current.in_list;

    ax::mojom::Role role = node->GetRole();
    bool current_is_link = in_link || (role == ax::mojom::Role::kLink);

    if (!in_list) {
      if (role == ax::mojom::Role::kHeading) {
        std::string text = node->GetTextContentUTF8();
        if (!text.empty()) {
          structure.headings.push_back(rust::String(text));
        }
      } else if (role == ax::mojom::Role::kList) {
        reading_mode::OriginalList current_list;
        for (const auto& child : node->children()) {
          if (child->GetRole() == ax::mojom::Role::kListItem) {
            std::string text = child->GetTextContentUTF8();
            if (!text.empty()) {
              current_list.items.push_back(rust::String(text));
            }
          }
        }
        if (!current_list.items.empty()) {
          structure.lists.push_back(current_list);
        }
      } else if (role == ax::mojom::Role::kBlockquote) {
        std::string text = node->GetTextContentUTF8();
        if (!text.empty()) {
          structure.blockquotes.push_back(rust::String(text));
        }
      }
    }

    if (role == ax::mojom::Role::kStaticText) {
      std::string text = node->GetTextContentUTF8();
      size_t len = text.length();
      structure.original_total_text_len += len;
      if (current_is_link) {
        structure.original_link_text_len += len;
      }

      if (!in_list && !text.empty()) {
        if (node->HasTextStyle(ax::mojom::TextStyle::kBold)) {
          structure.bold_fragments.push_back(rust::String(text));
        }
        if (node->HasTextStyle(ax::mojom::TextStyle::kItalic)) {
          structure.italic_fragments.push_back(rust::String(text));
        }
        if (inside_code || (node->parent() && node->parent()->GetRole() ==
                                                  ax::mojom::Role::kCode)) {
          structure.code_fragments.push_back(rust::String(text));
        }
      }
    }

    bool current_is_code = inside_code || (role == ax::mojom::Role::kCode);
    bool current_is_list = in_list || (role == ax::mojom::Role::kList);

    const auto& children = node->children();
    // Push children in reverse order to ensure leftmost child is popped and
    // processed first (matches canonical DFS left-to-right preorder
    // traversal).
    for (const auto& it : base::Reversed(children)) {
      stack.push_back(
          {it.get(), current_is_code, current_is_link, current_is_list});
    }
  }
}

}  // namespace

void ExtractOriginalStructureForTesting(ui::AXNode* root,
                                        OriginalStructure& structure,
                                        bool root_inside_code) {
  ExtractOriginalStructure(root, structure, root_inside_code,
                           /*root_in_link=*/false);
}

ReadingModeMetricsService::ReadingModeMetricsService(
    mojo::PendingReceiver<mojom::DistillationEvaluator> receiver)
    : receiver_(this, std::move(receiver)) {}

ReadingModeMetricsService::~ReadingModeMetricsService() = default;

void ReadingModeMetricsService::Evaluate(
    const ::ui::AXTreeUpdate& ax_tree_update,
    const std::string& distilled_html,
    EvaluateCallback callback) {
  if (ax_tree_update.nodes.empty()) {
    LOG(WARNING) << "Received empty AXTreeUpdate.";
    std::move(callback).Run(
        base::unexpected(mojom::EvaluationStatus::kEmptySnapshot));
    return;
  }

  ::ui::AXTree tree(ax_tree_update);
  if (!tree.root()) {
    LOG(WARNING) << "Failed to create AXTree from update or root is null.";
    std::move(callback).Run(
        base::unexpected(mojom::EvaluationStatus::kInvalidTreeStructure));
    return;
  }

  std::string original_text = tree.root()->GetTextContentUTF8();
  if (original_text.empty()) {
    LOG(WARNING) << "AXTree root has no text content.";
    std::move(callback).Run(
        base::unexpected(mojom::EvaluationStatus::kEmptyOriginalText));
    return;
  }

  reading_mode::OriginalStructure structure;
  ExtractOriginalStructure(tree.root(), structure, /*root_inside_code=*/false,
                           /*root_in_link=*/false);

  // Call the Rust evaluator to compute metrics.
  auto rust_metrics =
      evaluate(original_text.c_str(), distilled_html.c_str(), structure);

  if (!rust_metrics.well_formed) {
    LOG(WARNING) << "Distilled HTML is malformed (unclosed tags).";
    std::move(callback).Run(
        base::unexpected(mojom::EvaluationStatus::kMalformedDistilledHtml));
    return;
  }

  mojom::DistillationMetricsPtr metrics = mojom::DistillationMetrics::New();
  metrics->rouge_l_precision = rust_metrics.rouge_l_precision;
  metrics->rouge_l_recall = rust_metrics.rouge_l_recall;
  metrics->rouge_l_f1 = rust_metrics.rouge_l_f1;
  metrics->rouge_l_f2 = rust_metrics.rouge_l_f2;
  metrics->struct_score = rust_metrics.struct_score;
  metrics->format_score = rust_metrics.format_score;
  metrics->link_density_ratio = rust_metrics.link_density_ratio;

  std::move(callback).Run(base::ok(std::move(metrics)));
}

}  // namespace reading_mode
