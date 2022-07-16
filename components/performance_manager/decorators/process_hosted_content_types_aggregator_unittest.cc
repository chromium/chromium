// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_hosted_content_types_aggregator.h"

#include <memory>

#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {

class ProcessHostedContentTypesAggregatorTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ProcessHostedContentTypesAggregatorTest() = default;
  ~ProcessHostedContentTypesAggregatorTest() override = default;

  void SetUp() override {
    Super::SetUp();
    graph()->PassToGraph(
        std::make_unique<ProcessHostedContentTypesAggregator>());
  }
};

TEST_F(ProcessHostedContentTypesAggregatorTest, MainFrameAndChildFrame) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->is_visible());

  // Create the first process node. It is not hosting any main frames at first.
  auto process_node_1 = CreateNode<ProcessNodeImpl>();
  EXPECT_FALSE(process_node_1->hosted_content_types().Has(
      ProcessNode::ContentType::kMainFrame));

  // Create the first frame node. It is considered a main frame since it has no
  // parent frame.
  auto frame_node_1 =
      CreateFrameNodeAutoId(process_node_1.get(), page_node.get());
  EXPECT_TRUE(process_node_1->hosted_content_types().Has(
      ProcessNode::ContentType::kMainFrame));

  // Create a child frame node in another process.
  auto process_node_2 = CreateNode<ProcessNodeImpl>();
  EXPECT_FALSE(process_node_2->hosted_content_types().Has(
      ProcessNode::ContentType::kMainFrame));
  auto child_frame_node = CreateFrameNodeAutoId(
      process_node_2.get(), page_node.get(), frame_node_1.get());
  EXPECT_FALSE(process_node_2->hosted_content_types().Has(
      ProcessNode::ContentType::kMainFrame));
}

TEST_F(ProcessHostedContentTypesAggregatorTest, AdFrame) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->is_visible());

  // Create the first process node.
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());
  EXPECT_FALSE(
      process_node->hosted_content_types().Has(ProcessNode::ContentType::kAd));

  // Make it an ad frame.
  frame_node->SetIsAdFrame(true);
  EXPECT_TRUE(
      process_node->hosted_content_types().Has(ProcessNode::ContentType::kAd));

  // Untag the frame as an ad. The process is still counted as having hosted an
  // ad frame.
  frame_node->SetIsAdFrame(false);
  EXPECT_TRUE(
      process_node->hosted_content_types().Has(ProcessNode::ContentType::kAd));
}

TEST_F(ProcessHostedContentTypesAggregatorTest, ContentTypeIsPermanent) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->is_visible());

  // Create the first process node. It is not hosting any main frames at first.
  auto process_node = CreateNode<ProcessNodeImpl>();
  EXPECT_FALSE(process_node->hosted_content_types().Has(
      ProcessNode::ContentType::kMainFrame));

  // Create a frame node. It is considered a main frame since it has no parent
  // frame.
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());
  EXPECT_TRUE(process_node->hosted_content_types().Has(
      ProcessNode::ContentType::kMainFrame));

  // Remove the frame node. The process is still considered as having hosted a
  // main frame.
  frame_node.reset();
  EXPECT_TRUE(graph()->GetAllFrameNodes().empty());
  EXPECT_TRUE(process_node->hosted_content_types().Has(
      ProcessNode::ContentType::kMainFrame));
}

}  // namespace performance_manager
