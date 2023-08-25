// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_ax_tree_serializer.h"

#include <set>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id.h"

namespace screen_ai {

ScreenAIAXTreeSerializer::ScreenAIAXTreeSerializer(
    const ui::AXTreeID& parent_tree_id,
    std::vector<ui::AXNodeData>&& nodes)
    : tree_(std::make_unique<ui::AXSerializableTree>()) {
  ui::AXTreeUpdate update;
  if (!nodes.empty()) {
    DCHECK(base::Contains(
        std::set{ax::mojom::Role::kDialog, ax::mojom::Role::kRootWebArea,
                 ax::mojom::Role::kPdfRoot, ax::mojom::Role::kRegion},
        nodes[0].role))
        << nodes[0].role;
    update.root_id = nodes[0].id;
  } else {
    update.root_id = ui::kInvalidAXNodeID;
  }
  update.nodes = nodes;
  update.has_tree_data = true;
  update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data.parent_tree_id = parent_tree_id;
  update.tree_data.title = "Screen AI";

  if (!tree_->Unserialize(update))
    LOG(FATAL) << tree_->error();
  tree_source_ = base::WrapUnique<ui::AXTreeSource<const ui::AXNode*>>(
      tree_->CreateTreeSource());
  DCHECK(tree_source_);
  serializer_ = std::make_unique<
      ui::AXTreeSerializer<const ui::AXNode*, std::vector<const ui::AXNode*>>>(
      tree_source_.get(), /* crash_on_error */ true);
}

ScreenAIAXTreeSerializer::~ScreenAIAXTreeSerializer() = default;

ui::AXTreeUpdate ScreenAIAXTreeSerializer::Serialize() const {
  DCHECK(serializer_);
  DCHECK(tree_);
  ui::AXTreeUpdate out_update;
  if (!serializer_->SerializeChanges(tree_->root(), &out_update)) {
    NOTREACHED() << "Failure to serialize should have already caused the "
                    "process to crash due to the `crash_on_error` in "
                    "`AXTreeSerializer` constructor call.";
  }
  return out_update;
}

}  // namespace screen_ai
