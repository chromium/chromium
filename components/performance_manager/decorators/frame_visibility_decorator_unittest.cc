// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/frame_visibility_decorator.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/features.h"
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
// active.
TEST_F(FrameVisibilityDecoratorTest, IsActive) {
  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetIsVisible(true);
  auto main_frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());

  EXPECT_TRUE(main_frame_node->IsActive());
  EXPECT_EQ(main_frame_node->GetVisibility(), FrameNode::Visibility::kVisible);

  main_frame_node->SetIsActive(false);
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
  intersecting_child_frame_node->SetViewportIntersection(
      ViewportIntersection::kIntersecting);

  // Create a child frame node that doesn't intersect with the viewport.
  auto non_intersecting_child_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), main_frame_node.get());
  non_intersecting_child_frame_node->SetViewportIntersection(
      ViewportIntersection::kNotIntersecting);

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
  EXPECT_EQ(frame_node->GetViewportIntersection(),
            ViewportIntersection::kUnknown);

  // Create a child frame node whose intersection with the viewport is still
  // unknown.
  auto child_frame_node =
      CreateFrameNodeAutoId(process_node(), page_node.get(), frame_node.get());
  EXPECT_EQ(child_frame_node->GetViewportIntersection(),
            ViewportIntersection::kUnknown);

  // Both frames starts with an unknown visibility.
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);
  EXPECT_EQ(child_frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);

  // Make it so that the test frame intersects with the view port.
  frame_node->SetViewportIntersection(ViewportIntersection::kIntersecting);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(child_frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);

  // Make it so that the child frame intersects with the view port.
  child_frame_node->SetViewportIntersection(
      ViewportIntersection::kIntersecting);
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
  EXPECT_EQ(fenced_frame_node->GetViewportIntersection(),
            ViewportIntersection::kUnknown);
  EXPECT_EQ(fenced_frame_node->GetVisibility(),
            FrameNode::Visibility::kUnknown);

  // Make it so that the fenced frame intersects with the view port.
  fenced_frame_node->SetViewportIntersection(
      ViewportIntersection::kIntersecting);
  EXPECT_EQ(fenced_frame_node->GetVisibility(),
            FrameNode::Visibility::kVisible);
}

class FrameVisibilityDecoratorParentNotVisibleTest
    : public FrameVisibilityDecoratorTest,
      public testing::WithParamInterface<bool> {
 public:
  FrameVisibilityDecoratorParentNotVisibleTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kSubframeNotVisibleIfParentNotVisible, GetParam());
  }

 protected:
  // Expected visibility of a frame whose parent is not visible, when its own
  // viewport intersection gives `visibility_from_intersection`.
  FrameNode::Visibility ExpectedVisibilityUnderNotVisibleParent(
      FrameNode::Visibility visibility_from_intersection) const {
    return GetParam() ? FrameNode::Visibility::kNotVisible
                      : visibility_from_intersection;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FrameVisibilityDecoratorParentNotVisibleTest,
                         testing::Bool());

// Tests that a subframe of a frame that doesn't intersect with the viewport
// (e.g. a frame inside a `display:none` frame) is not visible when the feature
// is enabled, even if its own viewport intersection is unknown or intersecting.
TEST_P(FrameVisibilityDecoratorParentNotVisibleTest,
       SubframeOfNotVisibleFrame) {
  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetIsVisible(true);
  auto main_frame_node = CreateFrameNodeAutoId(process_node(), page_node.get());
  auto frame_node = CreateFrameNodeAutoId(process_node(), page_node.get(),
                                          main_frame_node.get());
  frame_node->SetViewportIntersection(ViewportIntersection::kNotIntersecting);
  ASSERT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);

  // Child frames created under a frame that is not visible.
  auto unknown_child_frame_node =
      CreateFrameNodeAutoId(process_node(), page_node.get(), frame_node.get());
  auto intersecting_child_frame_node =
      CreateFrameNodeAutoId(process_node(), page_node.get(), frame_node.get());
  intersecting_child_frame_node->SetViewportIntersection(
      ViewportIntersection::kIntersecting);
  // A grandchild, to verify that the effect propagates down the frame tree.
  auto grandchild_frame_node = CreateFrameNodeAutoId(
      process_node(), page_node.get(), intersecting_child_frame_node.get());
  grandchild_frame_node->SetViewportIntersection(
      ViewportIntersection::kIntersecting);

  EXPECT_EQ(
      unknown_child_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kUnknown));
  EXPECT_EQ(
      intersecting_child_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kVisible));
  EXPECT_EQ(
      grandchild_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kVisible));

  // The parent becomes visible: children get the visibility from their own
  // viewport intersection.
  frame_node->SetViewportIntersection(ViewportIntersection::kIntersecting);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
  EXPECT_EQ(unknown_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kUnknown);
  EXPECT_EQ(intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kVisible);
  EXPECT_EQ(grandchild_frame_node->GetVisibility(),
            FrameNode::Visibility::kVisible);

  // The parent becomes not visible again.
  frame_node->SetViewportIntersection(ViewportIntersection::kNotIntersecting);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(
      unknown_child_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kUnknown));
  EXPECT_EQ(
      intersecting_child_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kVisible));
  EXPECT_EQ(
      grandchild_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kVisible));

  // Hiding and showing the page preserves the effect of the parent.
  page_node->SetIsVisible(false);
  EXPECT_EQ(unknown_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(intersecting_child_frame_node->GetVisibility(),
            FrameNode::Visibility::kNotVisible);
  page_node->SetIsVisible(true);
  EXPECT_EQ(
      unknown_child_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kUnknown));
  EXPECT_EQ(
      intersecting_child_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kVisible));
  EXPECT_EQ(
      grandchild_frame_node->GetVisibility(),
      ExpectedVisibilityUnderNotVisibleParent(FrameNode::Visibility::kVisible));
}

}  // namespace performance_manager
