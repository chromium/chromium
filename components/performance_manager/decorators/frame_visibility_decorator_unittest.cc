// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_visibility_decorator.h"

#include <memory>

#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {

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

// Tests that a main frame in a visible page is not visible if it is not
// current.
TEST_F(FrameVisibilityDecoratorTest, IsCurrent) {
  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetIsVisible(true);
  auto main_frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  EXPECT_TRUE(main_frame_node->IsCurrent());
  EXPECT_EQ(main_frame_node->GetVisibility(), FrameNode::Visibility::kVisible);

  FrameNodeImpl::UpdateCurrentFrame(main_frame_node.get(), nullptr, graph());
  EXPECT_EQ(main_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
}

TEST_F(FrameVisibilityDecoratorTest, SetPageVisible) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->IsVisible());

  // Create a frame node.
  auto frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  // Starts not visible because the page is not visible.
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);

  // Make the page visible.
  page_node->SetIsVisible(true);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);

  // Make the page not visible again.
  page_node->SetIsVisible(false);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);
}

TEST_F(FrameVisibilityDecoratorTest, PageIsBeingMirrored) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->IsVisible());
  auto frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node.get())
      ->SetIsBeingMirroredForTesting(true);

  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
}

// Checks the interaction with PageLiveStateDecorator::Data::IsBeingMirrored and
// PageNode::IsVisible.
TEST_F(FrameVisibilityDecoratorTest, PageUserVisible) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->IsVisible());
  auto frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  // Frame starts not visible, and the page is neither visible or being
  // mirrored.
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);

  // Pretend the page starts getting mirrored.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node.get())
      ->SetIsBeingMirroredForTesting(true);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);

  // Now also set the IsVisible property. Stays visible.
  page_node->SetIsVisible(true);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);

  // Pretend the page is no longer getting mirrored. Stays visible.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node.get())
      ->SetIsBeingMirroredForTesting(false);

  // Set the IsVisible property to false. Now the frame becomes not visible.
  page_node->SetIsVisible(false);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);
}

TEST_F(FrameVisibilityDecoratorTest, SetPageVisibleWithChildNodes) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->IsVisible());

  // Create a main frame node.
  auto main_frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  // Create a child frame node whose intersection with the viewport is still
  // unknown.
  auto unknown_intersection_child_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), main_frame_node.get());

  // Create a child frame node that intersect with the viewport.
  auto intersecting_child_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), main_frame_node.get());
  intersecting_child_frame_node->SetViewportIntersectionForTesting(
      /*is_intersecting_viewport=*/true);

  // Create a child frame node that doesn't intersect with the viewport.
  auto non_intersecting_child_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), main_frame_node.get());
  non_intersecting_child_frame_node->SetViewportIntersectionForTesting(
      /*is_intersecting_viewport=*/false);

  // They all starts not visible because the page is not visible.
  EXPECT_EQ(unknown_intersection_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(main_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(non_intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);

  // Make the page visible.
  page_node->SetIsVisible(true);
  // The frame with an unknown viewport intersection has an unknown visibility.
  EXPECT_EQ(unknown_intersection_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kUnknown);
  // The frame that intersects with the viewport is now visible.
  EXPECT_EQ(main_frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kVisible);
  // The frame that doesn't intersect with the viewport is still not visible.
  EXPECT_EQ(non_intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);

  // Make the page not visible again.
  page_node->SetIsVisible(false);
  EXPECT_EQ(unknown_intersection_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(main_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(non_intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
}

TEST_F(FrameVisibilityDecoratorTest, SetFrameIntersectsViewport) {
  auto page_node = CreateNode<PageNodeImpl>();
  // The page starts already visible.
  page_node->SetIsVisible(true);
  auto main_frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  // Create a test frame node whose intersection with the viewport is still
  // unknown.
  auto frame_node = CreateFrameNodeAutoId(process_node(), page_node.get(),
                                          main_frame_node.get());
  EXPECT_FALSE(frame_node->GetViewportIntersection().has_value());

  // Create a child frame node whose intersection with the viewport is still
  // unknown.
  auto child_frame_node =
      CreateFrameNodeAutoId(process_node(), page_node.get(), frame_node.get());
  EXPECT_FALSE(child_frame_node->GetViewportIntersection().has_value());

  // Both frames starts with an unknown visibility.
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);
  EXPECT_EQ(child_frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);

  // Make it so that the test frame intersects with the view port.
  frame_node->SetViewportIntersectionForTesting(
      /*is_intersecting_viewport=*/true);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(child_frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);

  // Make it so that the child frame intersects with the view port.
  child_frame_node->SetViewportIntersectionForTesting(
      /*is_intersecting_viewport=*/true);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(child_frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
}

TEST_F(FrameVisibilityDecoratorTest, FencedFrame) {
  auto page_node = CreateNode<PageNodeImpl>();
  // The page starts already visible.
  page_node->SetIsVisible(true);

  // The main frame. No parent nor outer document.
  auto main_frame_node = CreateNode<FrameNodeImpl>(
      process_node(), page_node.get(), /*parent_frame_node=*/nullptr,
      /*outer_document_for_fenced_frame=*/nullptr,
      /*render_frame_id=*/1);

  // Create a <fencedframe> whose intersection with the viewport is still
  // unknown.
  auto fenced_frame_node = CreateNode<FrameNodeImpl>(
      process_node(), page_node.get(),
      /*parent_frame_node=*/nullptr,
      /*outer_document_for_fenced_frame=*/main_frame_node.get(),
      /*render_frame_id=*/2);
  EXPECT_FALSE(fenced_frame_node->GetViewportIntersection().has_value());
  EXPECT_EQ(fenced_frame_node->GetVisibility(),
            FrameNode::Visibility::kUnknown);

  // Make it so that the fenced frame intersects with the view port.
  fenced_frame_node->SetViewportIntersectionForTesting(
      /*is_intersecting_viewport=*/true);
  EXPECT_EQ(fenced_frame_node->GetVisibility(),
            FrameNode::Visibility::kVisible);
}

}  // namespace performance_manager
