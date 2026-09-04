// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/screen2x_distiller.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_update.h"

class Screen2xDistillerTest : public ChromeRenderViewTest {
 public:
  Screen2xDistillerTest() = default;
  ~Screen2xDistillerTest() override = default;

 protected:
  content::RenderFrame* GetRenderFrame() {
    return content::RenderFrame::FromWebFrame(GetMainFrame());
  }

  ui::AXSerializableTree CreateSimpleTree(
      ax::mojom::Role root_role = ax::mojom::Role::kRootWebArea,
      const ui::AXTreeID& tree_id = ui::AXTreeID::CreateNewAXTreeID()) {
    ui::AXTreeUpdate initial_state;
    initial_state.tree_data.tree_id = tree_id;
    ui::AXNodeData root;
    root.id = 1;
    root.role = root_role;
    initial_state.nodes = {root};
    initial_state.root_id = root.id;
    initial_state.has_tree_data = true;
    return ui::AXSerializableTree(initial_state);
  }
};

TEST_F(Screen2xDistillerTest, Distill_ValidTree_RunsDistillationAndCompletes) {
  // Instantiate the distiller with a completion callback to capture the result.
  std::optional<DistillationResult> captured_result;
  Screen2xDistiller distiller(
      GetRenderFrame(), base::BindRepeating([]() { return false; }),
      base::BindLambdaForTesting(
          [&](const DistillationResult& result) { captured_result = result; }));

  // Construct a sample accessibility tree for distillation.
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXSerializableTree tree =
      CreateSimpleTree(ax::mojom::Role::kRootWebArea, tree_id);

  // Execute distillation and verify the result payload and tree ID.
  DistillationRequest request;
  request.tree = &tree;
  request.ukm_source_id = ukm::kInvalidSourceId;

  distiller.Distill(request);
  ASSERT_TRUE(captured_result.has_value());
  EXPECT_EQ(captured_result->type, DistillationResult::Type::kAXNodeIds);
  EXPECT_EQ(captured_result->tree_id, tree_id);
}

TEST_F(Screen2xDistillerTest, ConsecutiveDistillations_CompletesActiveTree) {
  // Instantiate the distiller capturing all completed distillation results.
  std::vector<DistillationResult> captured_results;
  Screen2xDistiller distiller(
      GetRenderFrame(), base::BindRepeating([]() { return false; }),
      base::BindLambdaForTesting([&](const DistillationResult& result) {
        captured_results.push_back(result);
      }));

  // Create two distinct accessibility trees.
  ui::AXTreeID tree_id_1 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXSerializableTree tree1 =
      CreateSimpleTree(ax::mojom::Role::kRootWebArea, tree_id_1);

  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXSerializableTree tree2 =
      CreateSimpleTree(ax::mojom::Role::kRootWebArea, tree_id_2);

  // Trigger distillation on both trees sequentially and verify results.
  DistillationRequest request1;
  request1.tree = &tree1;
  distiller.Distill(request1);

  DistillationRequest request2;
  request2.tree = &tree2;
  distiller.Distill(request2);

  ASSERT_EQ(captured_results.size(), 2u);
  EXPECT_EQ(captured_results[0].tree_id, tree_id_1);
  EXPECT_EQ(captured_results[1].tree_id, tree_id_2);
}

TEST_F(Screen2xDistillerTest, Distill_QueriesScreenAIReadinessCallback) {
  // Set up a readiness callback to verify it gets queried during distillation.
  bool readiness_callback_invoked = false;
  Screen2xDistiller distiller(GetRenderFrame(),
                              base::BindLambdaForTesting([&]() {
                                readiness_callback_invoked = true;
                                return false;
                              }),
                              base::DoNothing());

  // Create an accessibility tree for distillation.
  ui::AXSerializableTree tree = CreateSimpleTree();

  // Trigger distillation and confirm the readiness callback was executed.
  DistillationRequest request;
  request.tree = &tree;

  distiller.Distill(request);
  EXPECT_TRUE(readiness_callback_invoked);
}

TEST_F(Screen2xDistillerTest, Distill_EmptyContent_CompletesWithEmptyNodeIds) {
  // Instantiate the distiller capturing the distillation result.
  std::optional<DistillationResult> captured_result;
  Screen2xDistiller distiller(
      GetRenderFrame(), base::BindRepeating([]() { return false; }),
      base::BindLambdaForTesting(
          [&](const DistillationResult& result) { captured_result = result; }));

  // Create an accessibility tree with no article or main content nodes.
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXSerializableTree tree =
      CreateSimpleTree(ax::mojom::Role::kGenericContainer, tree_id);

  // Execute distillation and verify it completes with an empty node ID list.
  DistillationRequest request;
  request.tree = &tree;

  distiller.Distill(request);
  ASSERT_TRUE(captured_result.has_value());
  EXPECT_EQ(captured_result->tree_id, tree_id);
  EXPECT_EQ(captured_result->type, DistillationResult::Type::kAXNodeIds);
  EXPECT_TRUE(captured_result->node_ids.empty());
}
