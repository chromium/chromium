// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl_operations.h"

#include <algorithm>

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class GraphImplOperationsTest : public GraphTestHarness {
 public:
  // Sets up two parallel frame trees that span multiple processes each.
  void SetUp() override {
    process1_ = CreateNode<ProcessNodeImpl>();
    process2_ = CreateNode<ProcessNodeImpl>();
    page1_ = CreateNode<PageNodeImpl>();
    page2_ = CreateNode<PageNodeImpl>();
    mainframe1_ =
        CreateFrameNodeAutoId(process1_.get(), page1_.get(), nullptr, 0);
    mainframe2_ =
        CreateFrameNodeAutoId(process2_.get(), page2_.get(), nullptr, 1);
    childframe1a_ = CreateFrameNodeAutoId(process2_.get(), page1_.get(),
                                          mainframe1_.get(), 2);
    childframe1b_ = CreateFrameNodeAutoId(process2_.get(), page1_.get(),
                                          mainframe1_.get(), 3);
    childframe2a_ = CreateFrameNodeAutoId(process1_.get(), page2_.get(),
                                          mainframe2_.get(), 4);
    childframe2b_ = CreateFrameNodeAutoId(process1_.get(), page2_.get(),
                                          mainframe2_.get(), 5);
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

}  // namespace

TEST_F(GraphImplOperationsTest, GetAssociatedPageNodes) {
  auto page_nodes =
      GraphImplOperations::GetAssociatedPageNodes(process1_.get());
  EXPECT_EQ(2u, page_nodes.size());
  EXPECT_THAT(page_nodes,
              testing::UnorderedElementsAre(page1_.get(), page2_.get()));
}

TEST_F(GraphImplOperationsTest, GetAssociatedProcessNodes) {
  auto process_nodes =
      GraphImplOperations::GetAssociatedProcessNodes(page1_.get());
  EXPECT_EQ(2u, process_nodes.size());
  EXPECT_THAT(process_nodes,
              testing::UnorderedElementsAre(process1_.get(), process2_.get()));
}

TEST_F(GraphImplOperationsTest, GetFrameNodes) {
  // Add a grandchild frame.
  auto grandchild = CreateFrameNodeAutoId(process1_.get(), page1_.get(),
                                          childframe1a_.get(), 6);

  auto frame_nodes = GraphImplOperations::GetFrameNodes(page1_.get());
  EXPECT_THAT(frame_nodes, testing::UnorderedElementsAre(
                               mainframe1_.get(), childframe1a_.get(),
                               childframe1b_.get(), grandchild.get()));
  // In a level order the main-frame is first, and the grandchild is last. The
  // two children can come in any order.
  EXPECT_EQ(mainframe1_.get(), frame_nodes[0]);
  EXPECT_EQ(grandchild.get(), frame_nodes[3]);
}

TEST_F(GraphImplOperationsTest, VisitFrameTree) {
  auto frame_nodes = GraphImplOperations::GetFrameNodes(page1_.get());

  std::vector<FrameNodeImpl*> visited;
  GraphImplOperations::VisitFrameTreePreOrder(
      page1_.get(), [&visited](FrameNodeImpl* frame_node) -> bool {
        visited.push_back(frame_node);
        return true;
      });
  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(
                  mainframe1_.get(), childframe1a_.get(), childframe1b_.get()));
  // In pre-order the main frame is first.
  EXPECT_EQ(mainframe1_.get(), visited[0]);

  visited.clear();
  GraphImplOperations::VisitFrameTreePostOrder(
      page1_.get(), [&visited](FrameNodeImpl* frame_node) -> bool {
        visited.push_back(frame_node);
        return true;
      });
  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(
                  mainframe1_.get(), childframe1a_.get(), childframe1b_.get()));
  // In post-order the main frame is last.
  EXPECT_EQ(mainframe1_.get(), visited[2]);
}

TEST_F(GraphImplOperationsTest, HasFrame) {
  EXPECT_TRUE(GraphImplOperations::HasFrame(page1_.get(), childframe1a_.get()));
  EXPECT_FALSE(
      GraphImplOperations::HasFrame(page1_.get(), childframe2a_.get()));
}

}  // namespace performance_manager
