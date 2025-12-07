// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_hosted_content_types_aggregator.h"

#include <memory>

#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "url/origin.h"

namespace performance_manager {

using ContentType = ProcessNode::ContentType;

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

  bool IsHosting(const TestNodeWrapper<ProcessNodeImpl>& process_node,
                 ContentType content_type) {
    return process_node->GetHostedContentTypes().Has(content_type);
  }
};

TEST_F(ProcessHostedContentTypesAggregatorTest,
       Extension_FrameCreatedAfterSetType) {
  // Create a process node.
  auto process_node = CreateNode<ProcessNodeImpl>();

  // Add an extension frame to it.
  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetType(PageType::kExtension);
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());

  EXPECT_TRUE(IsHosting(process_node, ContentType::kExtension));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kMainFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kWorker));

  // Remove the extension frame. The process is still counted as having hosted
  // an extension.
  frame_node.reset();
  page_node.reset();
  EXPECT_EQ(graph()->GetAllFrameNodes().size(), 0u);
  EXPECT_EQ(graph()->GetAllPageNodes().size(), 0u);

  EXPECT_TRUE(IsHosting(process_node, ContentType::kExtension));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kMainFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kWorker));
}

TEST_F(ProcessHostedContentTypesAggregatorTest,
       Extension_FrameCreatedBeforeSetType) {
  // Create a process node.
  auto process_node = CreateNode<ProcessNodeImpl>();

  // Create a page node with a main frame node.
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());

  // Set the page type after creating the frame.
  page_node->SetType(PageType::kExtension);

  EXPECT_TRUE(IsHosting(process_node, ContentType::kExtension));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kMainFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kWorker));
}

TEST_F(ProcessHostedContentTypesAggregatorTest, MainFrameAndChildFrame) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->IsVisible());

  // Create a main frame in a first process.
  auto process_node_1 = CreateNode<ProcessNodeImpl>();
  EXPECT_TRUE(process_node_1->GetHostedContentTypes().empty());
  auto main_frame_node =
      CreateFrameNodeAutoId(process_node_1.get(), page_node.get());

  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kExtension));
  EXPECT_TRUE(IsHosting(process_node_1, ContentType::kMainFrame));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kWorker));

  // Create a child frame node in another process.
  auto process_node_2 = CreateNode<ProcessNodeImpl>();
  EXPECT_TRUE(process_node_2->GetHostedContentTypes().empty());
  auto child_frame_node = CreateFrameNodeAutoId(
      process_node_2.get(), page_node.get(), main_frame_node.get());

  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kExtension));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kMainFrame));
  EXPECT_TRUE(IsHosting(process_node_2, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kWorker));

  // Remove the frames. This shouldn't affect hosted content types.
  child_frame_node.reset();
  main_frame_node.reset();
  EXPECT_EQ(graph()->GetAllFrameNodes().size(), 0u);

  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kExtension));
  EXPECT_TRUE(IsHosting(process_node_1, ContentType::kMainFrame));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node_1, ContentType::kWorker));

  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kExtension));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kMainFrame));
  EXPECT_TRUE(IsHosting(process_node_2, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node_2, ContentType::kWorker));
}

TEST_F(ProcessHostedContentTypesAggregatorTest, AdFrame) {
  auto page_node = CreateNode<PageNodeImpl>();

  // Create a main frame to host the ad frame.
  auto main_frame_process_node = CreateNode<ProcessNodeImpl>();
  auto main_frame_node =
      CreateFrameNodeAutoId(main_frame_process_node.get(), page_node.get());

  // Create an ad frame in another process.
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto ad_frame_node = CreateFrameNodeAutoId(
      process_node.get(), page_node.get(), main_frame_node.get());
  const GURL kUrl("https://example.com");
  ad_frame_node->OnNavigationCommitted(
      kUrl, url::Origin::Create(kUrl),
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  ad_frame_node->SetIsAdFrame(true);

  EXPECT_FALSE(IsHosting(process_node, ContentType::kExtension));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kMainFrame));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kSubframe));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kNavigatedFrame));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kWorker));

  // Untag the frame as an ad. The process is still counted as having hosted an
  // ad frame.
  ad_frame_node->SetIsAdFrame(false);

  EXPECT_FALSE(IsHosting(process_node, ContentType::kExtension));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kMainFrame));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kSubframe));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kNavigatedFrame));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kAd));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kWorker));
}

TEST_F(ProcessHostedContentTypesAggregatorTest, Worker) {
  // Create a worker node.
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto worker_node = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process_node.get());

  EXPECT_FALSE(IsHosting(process_node, ContentType::kExtension));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kMainFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kAd));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kWorker));

  // Remove the worker node. The process is still counted as having hosted a
  // worker.
  worker_node.reset();
  EXPECT_EQ(graph()->GetAllWorkerNodes().size(), 0u);

  EXPECT_FALSE(IsHosting(process_node, ContentType::kExtension));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kMainFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kSubframe));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kNavigatedFrame));
  EXPECT_FALSE(IsHosting(process_node, ContentType::kAd));
  EXPECT_TRUE(IsHosting(process_node, ContentType::kWorker));
}

}  // namespace performance_manager
