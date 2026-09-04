// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/screen2x_distiller.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/accessibility/ax_tree_update.h"

Screen2xDistiller::Screen2xDistiller(
    content::RenderFrame* render_frame,
    ScreenAIReadinessCallback is_screen_ai_ready_callback,
    DistillationCompleteCallback on_distillation_complete_callback)
    : is_screen_ai_ready_callback_(std::move(is_screen_ai_ready_callback)),
      on_distillation_complete_callback_(
          std::move(on_distillation_complete_callback)),
      distiller_(std::make_unique<AXTreeDistiller>(
          render_frame,
          base::BindRepeating(&Screen2xDistiller::OnAXTreeDistilled,
                              base::Unretained(this)))) {}

Screen2xDistiller::~Screen2xDistiller() = default;

void Screen2xDistiller::Distill(const DistillationRequest& request) {
  // Callers must ensure the tree is valid, has a root, and has a populated
  // tree ID before requesting distillation (e.g. ReadAnythingAppController
  // verifies this before invoking Distill). If any of these fail, something
  // is fundamentally wrong with the renderer state.
  CHECK(request.tree);
  CHECK_NE(request.tree->GetAXTreeID(), ui::AXTreeIDUnknown());
  CHECK(request.tree->root());

  if (is_screen_ai_ready_callback_ && is_screen_ai_ready_callback_.Run()) {
    distiller_->ScreenAIServiceReady();
  }

  std::unique_ptr<
      ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>>
      tree_source(request.tree->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*, std::vector<const ui::AXNode*>,
                       ui::AXTreeUpdate*, ui::AXTreeData*, ui::AXNodeData>
      serializer(tree_source.get());
  ui::AXTreeUpdate snapshot;
  CHECK(serializer.SerializeChanges(request.tree->root(), &snapshot));

  distiller_->Distill(*request.tree, snapshot, request.ukm_source_id);
}

void Screen2xDistiller::Reset() {
  // TODO(crbug.com/40802192): Implement in-flight distillation cancellation
  // in a future CL.
}

bool Screen2xDistiller::IsInProgress() const {
  // TODO(crbug.com/40802192): Implement in a future CL.
  return false;
}

void Screen2xDistiller::OnAXTreeDistilled(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  if (!on_distillation_complete_callback_) {
    return;
  }

  DistillationResult result;
  result.type = DistillationResult::Type::kAXNodeIds;
  result.tree_id = tree_id;
  result.node_ids = content_node_ids;
  on_distillation_complete_callback_.Run(result);
}
