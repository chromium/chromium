// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_distiller_factory.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_distiller.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_update.h"

class ReadAnythingDistillerFactoryTest : public ChromeRenderViewTest {
 public:
  ReadAnythingDistillerFactoryTest() = default;
  ~ReadAnythingDistillerFactoryTest() override = default;

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

TEST_F(ReadAnythingDistillerFactoryTest,
       CreateDistiller_Screen2x_CreatesNonNullDistiller) {
  ReadAnythingDistillerFactory factory(
      GetRenderFrame(), base::BindRepeating([]() { return false; }));

  std::unique_ptr<ReadAnythingDistiller> distiller = factory.CreateDistiller(
      ReadAnythingAppModel::DistillationMethod::kScreen2x, base::DoNothing());

  EXPECT_NE(distiller, nullptr);
}

TEST_F(ReadAnythingDistillerFactoryTest,
       CreateDistiller_Screen2x_DistillsSuccessfully) {
  std::optional<DistillationResult> captured_result;
  ReadAnythingDistillerFactory factory(
      GetRenderFrame(), base::BindRepeating([]() { return false; }));

  std::unique_ptr<ReadAnythingDistiller> distiller = factory.CreateDistiller(
      ReadAnythingAppModel::DistillationMethod::kScreen2x,
      base::BindLambdaForTesting(
          [&](const DistillationResult& result) { captured_result = result; }));
  ASSERT_NE(distiller, nullptr);

  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXSerializableTree tree =
      CreateSimpleTree(ax::mojom::Role::kRootWebArea, tree_id);

  DistillationRequest request;
  request.tree = &tree;
  request.ukm_source_id = ukm::kInvalidSourceId;

  distiller->Distill(request);
  ASSERT_TRUE(captured_result.has_value());
  EXPECT_EQ(captured_result->type, DistillationResult::Type::kAXNodeIds);
  EXPECT_EQ(captured_result->tree_id, tree_id);
}

TEST_F(ReadAnythingDistillerFactoryTest,
       CreateDistiller_Screen2x_ForwardsReadinessCallback) {
  bool readiness_callback_invoked = false;
  ReadAnythingDistillerFactory factory(GetRenderFrame(),
                                       base::BindLambdaForTesting([&]() {
                                         readiness_callback_invoked = true;
                                         return false;
                                       }));

  std::unique_ptr<ReadAnythingDistiller> distiller = factory.CreateDistiller(
      ReadAnythingAppModel::DistillationMethod::kScreen2x, base::DoNothing());
  ASSERT_NE(distiller, nullptr);

  ui::AXSerializableTree tree = CreateSimpleTree();
  DistillationRequest request;
  request.tree = &tree;

  distiller->Distill(request);
  EXPECT_TRUE(readiness_callback_invoked);
}
