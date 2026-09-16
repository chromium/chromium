// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph_operations.h"

#include <algorithm>

#include "base/functional/function_ref.h"
#include "base/test/gtest_util.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class GraphOperationsTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  // Sets up two parallel frame trees that span multiple processes each.
  void SetUp() override {
    Super::SetUp();
    process1_ = CreateNode<ProcessNodeImpl>();
    process2_ = CreateNode<ProcessNodeImpl>();
    page1_ = CreateNode<PageNodeImpl>();
    page2_ = CreateNode<PageNodeImpl>();
    mainframe1_ = CreateFrameNodeAutoId(process1_.get(), page1_.get(), nullptr);
    mainframe2_ = CreateFrameNodeAutoId(process2_.get(), page2_.get(), nullptr);
    childframe1a_ =
        CreateFrameNodeAutoId(process2_.get(), page1_.get(), mainframe1_.get());
    childframe1b_ =
        CreateFrameNodeAutoId(process2_.get(), page1_.get(), mainframe1_.get());
    childframe2a_ =
        CreateFrameNodeAutoId(process1_.get(), page2_.get(), mainframe2_.get());
    childframe2b_ =
        CreateFrameNodeAutoId(process1_.get(), page2_.get(), mainframe2_.get());
  }

  TestNodeWrapper<ProcessNodeImpl> process1_;
  TestNodeWrapper<ProcessNodeImpl> process2_;
  TestNodeWrapper<PageNodeImpl> page1_;
  TestNodeWrapper<PageNodeImpl> page2_;

  // Root nodes. |mainframeX_| is in |processX_|.
  TestNodeWrapper<FrameNodeImpl> mainframe1_;
  TestNodeWrapper<FrameNodeImpl> mainframe2_;

  // Children of |mainframe1_|, but in |process2_|.
  TestNodeWrapper<FrameNodeImpl> childframe1a_;
  TestNodeWrapper<FrameNodeImpl> childframe1b_;

  // Children of |mainframe2_|, but in |process1_|.
  TestNodeWrapper<FrameNodeImpl> childframe2a_;
  TestNodeWrapper<FrameNodeImpl> childframe2b_;
};

const PageNode* ToPublic(PageNodeImpl* page_node) {
  return page_node;
}

const FrameNode* ToPublic(FrameNodeImpl* frame_node) {
  return frame_node;
}

}  // namespace

TEST_F(GraphOperationsTest, GetAssociatedPageNodes) {
  auto page_nodes = GraphOperations::GetAssociatedPageNodes(process1_.get());
  EXPECT_EQ(2u, page_nodes.size());
  EXPECT_THAT(page_nodes, testing::UnorderedElementsAre(
                              ToPublic(page1_.get()), ToPublic(page2_.get())));
}

TEST_F(GraphOperationsTest, GetAssociatedProcessNodes) {
  auto process_nodes = GraphOperations::GetAssociatedProcessNodes(page1_.get());
  EXPECT_EQ(2u, process_nodes.size());
  EXPECT_THAT(process_nodes,
              testing::UnorderedElementsAre(process1_.get(), process2_.get()));
}

TEST_F(GraphOperationsTest, GetFrameNodes) {
  // Add a grandchild frame.
  auto grandchild =
      CreateFrameNodeAutoId(process1_.get(), page1_.get(), childframe1a_.get());

  auto frame_nodes = GraphOperations::GetFrameNodes(page1_.get());
  EXPECT_THAT(frame_nodes,
              testing::UnorderedElementsAre(
                  ToPublic(mainframe1_.get()), ToPublic(childframe1a_.get()),
                  ToPublic(childframe1b_.get()), ToPublic(grandchild.get())));
  // In a level order the main-frame is first, and the grandchild is last. The
  // two children can come in any order.
  EXPECT_EQ(ToPublic(mainframe1_.get()), frame_nodes[0]);
  EXPECT_EQ(ToPublic(grandchild.get()), frame_nodes[3]);
}

TEST_F(GraphOperationsTest, VisitFrameTree) {
  auto frame_nodes = GraphOperations::GetFrameNodes(page1_.get());

  std::vector<const FrameNode*> visited;
  EXPECT_TRUE(GraphOperations::VisitFrameTreePreOrder(
      page1_.get(), [&visited](const FrameNode* frame_node) -> bool {
        visited.push_back(frame_node);
        return true;
      }));
  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(ToPublic(mainframe1_.get()),
                                            ToPublic(childframe1a_.get()),
                                            ToPublic(childframe1b_.get())));
  // In pre-order the main frame is first.
  EXPECT_EQ(ToPublic(mainframe1_.get()), visited[0]);

  // Do an aborted pre-order visit.
  visited.clear();
  EXPECT_FALSE(GraphOperations::VisitFrameTreePreOrder(
      page1_.get(), [&visited](const FrameNode* frame_node) -> bool {
        visited.push_back(frame_node);
        return false;
      }));
  EXPECT_EQ(1u, visited.size());

  visited.clear();
  EXPECT_TRUE(GraphOperations::VisitFrameTreePostOrder(
      page1_.get(), [&visited](const FrameNode* frame_node) -> bool {
        visited.push_back(frame_node);
        return true;
      }));
  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(ToPublic(mainframe1_.get()),
                                            ToPublic(childframe1a_.get()),
                                            ToPublic(childframe1b_.get())));
  // In post-order the main frame is last.
  EXPECT_EQ(mainframe1_.get(), visited[2]);

  // Do an aborted post-order visit.
  visited.clear();
  EXPECT_FALSE(GraphOperations::VisitFrameTreePostOrder(
      page1_.get(), [&visited](const FrameNode* frame_node) -> bool {
        visited.push_back(frame_node);
        return false;
      }));
  EXPECT_EQ(1u, visited.size());
}

TEST_F(GraphOperationsTest, HasFrame) {
  EXPECT_TRUE(GraphOperations::HasFrame(page1_.get(), childframe1a_.get()));
  EXPECT_FALSE(GraphOperations::HasFrame(page1_.get(), childframe2a_.get()));
}

TEST_F(GraphOperationsTest, GetActiveFrameForFrameTreeNodeId) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();

  const content::FrameTreeNodeId kMainFtnId =
      content::FrameTreeNodeId::FromUnsafeValue(10);
  const content::FrameTreeNodeId kChildFtnId =
      content::FrameTreeNodeId::FromUnsafeValue(20);
  const content::FrameTreeNodeId kNonExistentFtnId =
      content::FrameTreeNodeId::FromUnsafeValue(99);

  // Create an active main frame.
  auto main_frame = CreateFrameNodeAutoId(
      process.get(), page.get(), /*parent_frame_node=*/nullptr,
      content::BrowsingInstanceId(), kMainFtnId);

  // Create an inactive (speculative) main frame with the same FrameTreeNodeId.
  auto speculative_main_frame = CreateSpeculativeFrameNodeAutoId(
      process.get(), page.get(), /*parent_frame_node=*/nullptr,
      content::BrowsingInstanceId(), kMainFtnId);

  // Create an active child frame.
  auto child_frame =
      CreateFrameNodeAutoId(process.get(), page.get(), main_frame.get(),
                            content::BrowsingInstanceId(), kChildFtnId);

  // Create an inactive (speculative) child frame with the same FrameTreeNodeId.
  auto speculative_child_frame = CreateSpeculativeFrameNodeAutoId(
      process.get(), page.get(), main_frame.get(),
      content::BrowsingInstanceId(), kChildFtnId);

  // Verify GetFrameTreeNodeId on each frame.
  EXPECT_EQ(main_frame->GetFrameTreeNodeId(), kMainFtnId);
  EXPECT_EQ(speculative_main_frame->GetFrameTreeNodeId(), kMainFtnId);
  EXPECT_EQ(child_frame->GetFrameTreeNodeId(), kChildFtnId);
  EXPECT_EQ(speculative_child_frame->GetFrameTreeNodeId(), kChildFtnId);

  // Lookup active frames on the page (public API).
  EXPECT_EQ(
      GraphOperations::GetActiveFrameForFrameTreeNodeId(page.get(), kMainFtnId),
      main_frame.get());
  EXPECT_EQ(GraphOperations::GetActiveFrameForFrameTreeNodeId(page.get(),
                                                              kChildFtnId),
            child_frame.get());
  EXPECT_EQ(GraphOperations::GetActiveFrameForFrameTreeNodeId(
                page.get(), kNonExistentFtnId),
            nullptr);

  // Lookup active frames on the page (internal API).
  EXPECT_EQ(GraphImplOperations::GetActiveFrameForFrameTreeNodeId(page.get(),
                                                                  kMainFtnId),
            main_frame.get());
  EXPECT_EQ(GraphImplOperations::GetActiveFrameForFrameTreeNodeId(page.get(),
                                                                  kChildFtnId),
            child_frame.get());
  EXPECT_EQ(GraphImplOperations::GetActiveFrameForFrameTreeNodeId(
                page.get(), kNonExistentFtnId),
            nullptr);

  // Once the old frame is deactivated and the speculative frame becomes active,
  // the lookup returns the new active frame.
  main_frame->SetIsActive(false);
  speculative_main_frame->SetIsActive(true);
  EXPECT_EQ(
      GraphOperations::GetActiveFrameForFrameTreeNodeId(page.get(), kMainFtnId),
      speculative_main_frame.get());
  EXPECT_EQ(GraphImplOperations::GetActiveFrameForFrameTreeNodeId(page.get(),
                                                                  kMainFtnId),
            speculative_main_frame.get());

  child_frame->SetIsActive(false);
  speculative_child_frame->SetIsActive(true);
  EXPECT_EQ(GraphOperations::GetActiveFrameForFrameTreeNodeId(page.get(),
                                                              kChildFtnId),
            speculative_child_frame.get());
  EXPECT_EQ(GraphImplOperations::GetActiveFrameForFrameTreeNodeId(page.get(),
                                                                  kChildFtnId),
            speculative_child_frame.get());
}

}  // namespace performance_manager
