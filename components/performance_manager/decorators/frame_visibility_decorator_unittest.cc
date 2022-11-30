// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_visibility_decorator.h"

#include <memory>

#include "components/performance_manager/test_support/graph_test_harness.h"
#include "ui/gfx/geometry/rect.h"

namespace performance_manager {

namespace {

static constexpr gfx::Rect kEmptyIntersection(0, 0, 0, 0);
static constexpr gfx::Rect kNonEmptyIntersection(0, 0, 10, 10);

}  // namespace

class FrameVisibilityDecoratorTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FrameVisibilityDecoratorTest() = default;
  ~FrameVisibilityDecoratorTest() override = default;

  void SetUp() override {
    Super::SetUp();
    graph()->PassToGraph(std::make_unique<FrameVisibilityDecorator>());

    process_node_ = CreateNode<ProcessNodeImpl>();
  }

  ProcessNodeImpl* process_node() const { return process_node_.get(); }

 private:
  TestNodeWrapper<ProcessNodeImpl> process_node_;
};

TEST_F(FrameVisibilityDecoratorTest, SetPageVisible) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->is_visible());

  // Create a frame node.
  auto frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  // Starts not visible because the page is not visible.
  EXPECT_EQ(frame_node->visibility(), FrameNode::Visibility::kNotVisible);

  // Make the page visible.
  page_node->SetIsVisible(true);
  EXPECT_EQ(frame_node->visibility(), FrameNode::Visibility::kVisible);

  // Make the page not visible again.
  page_node->SetIsVisible(false);
  EXPECT_EQ(frame_node->visibility(), FrameNode::Visibility::kNotVisible);
}

TEST_F(FrameVisibilityDecoratorTest, SetPageVisibleWithChildNodes) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->is_visible());

  // Create a main frame node.
  auto main_frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  // Create a child frame node with a non-empty viewport intersection.
  auto intersecting_child_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), main_frame_node.get());
  intersecting_child_frame_node->SetViewportIntersection(kNonEmptyIntersection);

  // Create a child frame node with an empty viewport intersection.
  auto non_intersecting_child_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), main_frame_node.get());
  non_intersecting_child_frame_node->SetViewportIntersection(
      kEmptyIntersection);

  // Create a child frame node with no viewport intersection
  auto no_intersection_child_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), main_frame_node.get());

  // Starts not visible because the page is not visible.
  EXPECT_EQ(main_frame_node->visibility(), FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(intersecting_child_frame_node->visibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(non_intersecting_child_frame_node->visibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(no_intersection_child_frame_node->visibility(),
            FrameNode::Visibility::kNotVisible);

  // Make the page visible.
  page_node->SetIsVisible(true);
  EXPECT_EQ(main_frame_node->visibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(intersecting_child_frame_node->visibility(),
            FrameNode::Visibility::kVisible);
  // The frame with an empty viewport intersection is still not visible.
  EXPECT_EQ(non_intersecting_child_frame_node->visibility(),
            FrameNode::Visibility::kNotVisible);
  // The frame with no viewport intersection has an unknown visibility.
  EXPECT_EQ(no_intersection_child_frame_node->visibility(),
            FrameNode::Visibility::kUnknown);

  // Make the page not visible again.
  page_node->SetIsVisible(false);
  EXPECT_EQ(main_frame_node->visibility(), FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(intersecting_child_frame_node->visibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(non_intersecting_child_frame_node->visibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(no_intersection_child_frame_node->visibility(),
            FrameNode::Visibility::kNotVisible);
}

TEST_F(FrameVisibilityDecoratorTest, SetFrameViewportIntersection) {
  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetIsVisible(true);
  auto main_frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  // Create a test frame node with no viewport intersection.
  auto frame_node = CreateFrameNodeAutoId(process_node(), page_node.get(),
                                          main_frame_node.get());
  EXPECT_FALSE(frame_node->viewport_intersection().has_value());

  // Create a child frame node with no viewport_intersection.
  auto child_frame_node =
      CreateFrameNodeAutoId(process_node(), page_node.get(), frame_node.get());
  EXPECT_FALSE(child_frame_node->viewport_intersection().has_value());

  // Both frames have an unknown visibility because their viewport intersection
  // hasn't been determined yet.
  EXPECT_EQ(frame_node->visibility(), FrameNode::Visibility::kUnknown);
  EXPECT_EQ(child_frame_node->visibility(), FrameNode::Visibility::kUnknown);

  // Set the viewport intersection of the test frame to a non-empty one.
  frame_node->SetViewportIntersection(kNonEmptyIntersection);
  EXPECT_EQ(frame_node->visibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(child_frame_node->visibility(), FrameNode::Visibility::kUnknown);

  // Set the viewport intersection of the child frame to a non-empty one.
  child_frame_node->SetViewportIntersection(kNonEmptyIntersection);
  EXPECT_EQ(frame_node->visibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(child_frame_node->visibility(), FrameNode::Visibility::kVisible);
}

}  // namespace performance_manager
