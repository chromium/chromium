// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph_operations.h"

#include <algorithm>

#include "base/functional/function_ref.h"
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
    page3_ = CreateNode<PageNodeImpl>();
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
    page3_->SetEmbedderFrameNodeAndEmbeddingType(
        mainframe1_.get(), PageNode::EmbeddingType::kGuestView);
  }

  TestNodeWrapper<ProcessNodeImpl> process1_;
  TestNodeWrapper<ProcessNodeImpl> process2_;
  TestNodeWrapper<PageNodeImpl> page1_;
  TestNodeWrapper<PageNodeImpl> page2_;
  TestNodeWrapper<PageNodeImpl> page3_;  // A guest of `page1_`.

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

TEST_F(GraphOperationsTest, VisitPageEmbeds) {
  // Pages are visited embedder-to-embedded.
  std::vector<const PageNode*> visited;
  ASSERT_TRUE(GraphOperations::VisitPageAndEmbedsPreOrder(
      page1_.get(), [&visited](const PageNode* page_node) {
        visited.push_back(page_node);
        return true;
      }));
  EXPECT_THAT(visited, testing::ElementsAre(ToPublic(page1_.get()),
                                            ToPublic(page3_.get())));

  // Stop after the first item.
  visited.clear();
  ASSERT_FALSE(GraphOperations::VisitPageAndEmbedsPreOrder(
      page1_.get(), [&visited](const PageNode* page_node) {
        visited.push_back(page_node);
        return false;
      }));
  EXPECT_THAT(visited, testing::ElementsAre(ToPublic(page1_.get())));
}

TEST_F(GraphOperationsTest, HasFrame) {
  EXPECT_TRUE(GraphOperations::HasFrame(page1_.get(), childframe1a_.get()));
  EXPECT_FALSE(GraphOperations::HasFrame(page1_.get(), childframe2a_.get()));
}

}  // namespace performance_manager
