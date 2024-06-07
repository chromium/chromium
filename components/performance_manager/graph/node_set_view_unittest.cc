// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_set_view.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

template <class UnderlyingSetAndNodeViewPtrPair>
class NodeSetViewTest : public GraphTestHarness {
 public:
  using UnderlyingSet = UnderlyingSetAndNodeViewPtrPair::first_type;
  using NodeViewPtr = UnderlyingSetAndNodeViewPtrPair::second_type;

  NodeSetViewTest() = default;

  void SetUp() override {
    GraphTestHarness::SetUp();

    process_node_ = CreateRendererProcessNode();
    page_node_ = CreateNode<PageNodeImpl>();
  }

  void TearDown() override {
    frame_nodes_.clear();
    page_node_.reset();
    process_node_.reset();

    GraphTestHarness::TearDown();
  }

  // Creates `n` frame nodes, store them in `frame_nodes_` and return a set
  // containing the pointer of every created frame.
  UnderlyingSet CreateFrameNodeSet(size_t n) {
    frame_nodes_.reserve(n);

    for (size_t i = 0; i < n; i++) {
      frame_nodes_.push_back(
          CreateFrameNodeAutoId(process_node_.get(), page_node_.get()));
    }

    UnderlyingSet result;
    for (const auto& frame_node : frame_nodes_) {
      result.insert(frame_node.get());
    }
    return result;
  }

 private:
  // The renderer and the page hosting the frames.
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
  std::vector<TestNodeWrapper<FrameNodeImpl>> frame_nodes_;
};

}  // namespace

TYPED_TEST_SUITE_P(NodeSetViewTest);

TYPED_TEST_P(NodeSetViewTest, Iterator) {
  using UnderlyingSet = TypeParam::first_type;
  using NodeViewPtr = TypeParam::second_type;

  // Create the node set containing frame nodes.
  const UnderlyingSet node_set = this->CreateFrameNodeSet(20);

  // Create the view over the node set.
  NodeSetView<UnderlyingSet, NodeViewPtr> node_set_view(node_set);

  // Iteration using range-based for loop works.
  for (NodeViewPtr node : node_set_view) {
    ASSERT_TRUE(node);
    ASSERT_TRUE(base::Contains(node_set, node));
  }

  // Compare the contents of the view to the contents of the node set.
  EXPECT_THAT(node_set, ::testing::UnorderedElementsAreArray(
                            node_set_view.begin(), node_set_view.end()));
}

TYPED_TEST_P(NodeSetViewTest, AsVector) {
  using UnderlyingSet = TypeParam::first_type;
  using NodeViewPtr = TypeParam::second_type;

  // Create the node set containing frame nodes.
  const UnderlyingSet node_set = this->CreateFrameNodeSet(20);

  // Create the view over the node set.
  NodeSetView<UnderlyingSet, NodeViewPtr> node_set_view(node_set);

  // Use NodeSetView::AsVector().
  std::vector<NodeViewPtr> nodes_from_view = node_set_view.AsVector();

  // Compare the contents of the vector to the contents of the node set.
  EXPECT_THAT(node_set, ::testing::UnorderedElementsAreArray(
                            nodes_from_view.begin(), nodes_from_view.end()));
}

REGISTER_TYPED_TEST_SUITE_P(NodeSetViewTest, Iterator, AsVector);

using TestedTypes = ::testing::Types<
    std::pair<std::unordered_set<const Node*>, const FrameNode*>,
    std::pair<std::unordered_set<const Node*>, FrameNodeImpl*>,
    std::pair<base::flat_set<const Node*>, const FrameNode*>,
    std::pair<base::flat_set<const Node*>, FrameNodeImpl*>>;

INSTANTIATE_TYPED_TEST_SUITE_P(My, NodeSetViewTest, TestedTypes);

}  // namespace performance_manager
