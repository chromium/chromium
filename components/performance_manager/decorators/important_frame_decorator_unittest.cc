// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/important_frame_decorator.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"

namespace performance_manager {

class ImportantFrameDecoratorTest : public GraphTestHarness {
 public:
  ImportantFrameDecoratorTest()
      : scoped_feature_list_(features::kUnimportantFramesPriority) {}
  ~ImportantFrameDecoratorTest() override = default;

  void OnGraphCreated(GraphImpl* graph) override {
    graph->PassToGraph(std::make_unique<ImportantFrameDecorator>());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ImportantFrameDecoratorTest, HadUserActivation) {
  // Create a graph with a child frame as only child frames can be unimportant.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  auto& frame_node = mock_graph.child_frame;

  // Intersects with a small area of the viewport. Not important.
  frame_node->SetViewportIntersection(ViewportIntersection::kIntersecting);
  frame_node->SetViewportIntersectionArea(
      ViewportIntersectionArea::kSmallOrNone);

  // No user activation. Not important.
  EXPECT_FALSE(frame_node->HadUserActivation());
  EXPECT_FALSE(frame_node->IsImportant());

  // User activation. Important.
  frame_node->SetHadUserActivation();
  EXPECT_TRUE(frame_node->IsImportant());
}

TEST_F(ImportantFrameDecoratorTest,
       ViewportIntersectionAndViewportIntersectionArea) {
  // Create a graph with a child frame as only child frames can be unimportant.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  auto& frame_node = mock_graph.child_frame;

  EXPECT_FALSE(frame_node->HadUserActivation());

  // Unknown viewport intersection: `GetViewportIntersectionArea()` is clamped
  // to `kUnknown` and important is assumed, regardless of any
  // `SetViewportIntersectionArea()` calls.
  EXPECT_EQ(frame_node->GetViewportIntersection(),
            ViewportIntersection::kUnknown);
  EXPECT_EQ(frame_node->GetViewportIntersectionArea(),
            ViewportIntersectionArea::kUnknown);
  EXPECT_TRUE(frame_node->IsImportant());

  frame_node->SetViewportIntersectionArea(
      ViewportIntersectionArea::kSmallOrNone);
  EXPECT_TRUE(frame_node->IsImportant());

  frame_node->SetViewportIntersectionArea(ViewportIntersectionArea::kLarge);
  EXPECT_TRUE(frame_node->IsImportant());

  // Not rendered: never important, regardless of
  // `SetViewportIntersectionArea()`.
  frame_node->SetViewportIntersection(ViewportIntersection::kNotRendered);
  EXPECT_FALSE(frame_node->IsImportant());

  frame_node->SetViewportIntersectionArea(
      ViewportIntersectionArea::kSmallOrNone);
  EXPECT_FALSE(frame_node->IsImportant());

  frame_node->SetViewportIntersectionArea(ViewportIntersectionArea::kLarge);
  EXPECT_FALSE(frame_node->IsImportant());

  // Rendered outside the viewport: never important, regardless of
  // `SetViewportIntersectionArea()`.
  frame_node->SetViewportIntersection(
      ViewportIntersection::kRenderedOutOfViewport);
  EXPECT_FALSE(frame_node->IsImportant());

  frame_node->SetViewportIntersectionArea(
      ViewportIntersectionArea::kSmallOrNone);
  EXPECT_FALSE(frame_node->IsImportant());

  frame_node->SetViewportIntersectionArea(ViewportIntersectionArea::kLarge);
  EXPECT_FALSE(frame_node->IsImportant());

  // Intersects with the viewport. It'll be important if it intersects with a
  // large area of the viewport.
  frame_node->SetViewportIntersection(ViewportIntersection::kIntersecting);

  frame_node->SetViewportIntersectionArea(
      ViewportIntersectionArea::kSmallOrNone);
  EXPECT_FALSE(frame_node->IsImportant());

  frame_node->SetViewportIntersectionArea(ViewportIntersectionArea::kLarge);
  EXPECT_TRUE(frame_node->IsImportant());
}

}  // namespace performance_manager
