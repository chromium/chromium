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

  // Intersects with a non-large area of the viewport. Not important.
  frame_node->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateIntersecting(
          /*is_intersecting_large_area*/ false));

  // No user activation. Not important.
  EXPECT_FALSE(frame_node->HadUserActivation());
  EXPECT_FALSE(frame_node->IsImportant());

  // User activation. Important.
  frame_node->SetHadUserActivation();
  EXPECT_TRUE(frame_node->IsImportant());
}

TEST_F(ImportantFrameDecoratorTest, ViewportIntersection) {
  // Create a graph with a child frame as only child frames can be unimportant.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  auto& frame_node = mock_graph.child_frame;

  EXPECT_FALSE(frame_node->HadUserActivation());

  // No viewport intersection yet. Important is assumed.
  EXPECT_EQ(frame_node->GetViewportIntersection(), std::nullopt);
  EXPECT_TRUE(frame_node->IsImportant());

  // Does not intersect with the viewport. Not important.
  frame_node->SetViewportIntersectionForTesting(
      /*intersects_with_viewport=*/false);
  EXPECT_FALSE(frame_node->IsImportant());

  // Intersects with a non-large area of the viewport. Not important.
  frame_node->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateIntersecting(
          /*is_intersecting_large_area*/ false));
  EXPECT_FALSE(frame_node->IsImportant());

  // Intersects with a large area of the viewport. Important.
  frame_node->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateIntersecting(
          /*is_intersecting_large_area*/ true));
  EXPECT_TRUE(frame_node->IsImportant());
}

}  // namespace performance_manager
