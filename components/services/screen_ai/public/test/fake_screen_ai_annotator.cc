// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/test/fake_screen_ai_annotator.h"

#include <utility>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace screen_ai::test {

FakeScreenAIAnnotator::FakeScreenAIAnnotator(bool create_empty_result)
    : create_empty_result_(create_empty_result) {}

FakeScreenAIAnnotator::~FakeScreenAIAnnotator() = default;

void FakeScreenAIAnnotator::PerformOcrAndReturnAXTreeUpdate(
    const ::SkBitmap& image,
    PerformOcrAndReturnAXTreeUpdateCallback callback) {
  ui::AXTreeUpdate update;
  if (!create_empty_result_) {
    update.root_id = next_node_id_;
    ui::AXNodeData node;
    node.id = next_node_id_;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked("Testing");
    update.nodes = {node};
    --next_node_id_;
  }
  std::move(callback).Run(update);
}

void FakeScreenAIAnnotator::ExtractSemanticLayout(
    const ::SkBitmap& image,
    const ::ui::AXTreeID& parent_tree_id,
    ExtractSemanticLayoutCallback callback) {
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  std::move(callback).Run(tree_id);
}

void FakeScreenAIAnnotator::PerformOcrAndReturnAnnotation(
    const ::SkBitmap& image,
    PerformOcrAndReturnAnnotationCallback callback) {
  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  std::move(callback).Run(std::move(annotation));
}

mojo::PendingRemote<mojom::ScreenAIAnnotator>
FakeScreenAIAnnotator::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace screen_ai::test
