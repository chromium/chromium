// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/extension_service_worker_voter.h"

#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/test_worker_node_factory.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const WorkerNode* worker_node) {
  return execution_context::ExecutionContext::From(worker_node);
}

class ExtensionServiceWorkerVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ExtensionServiceWorkerVoterTest() {
    scoped_feature_list_.InitWithFeatures(
        {performance_manager::features::kExtensionServiceWorkerVoter},
        {} /* disabled_features */);
  }

  ~ExtensionServiceWorkerVoterTest() override = default;

  ExtensionServiceWorkerVoterTest(const ExtensionServiceWorkerVoterTest&) =
      delete;
  ExtensionServiceWorkerVoterTest& operator=(
      const ExtensionServiceWorkerVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    extension_service_worker_voter_.InitializeOnGraph(
        graph(), observer_.BuildVotingChannel());
  }

  void TearDown() override {
    extension_service_worker_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  VoterId voter_id() const {
    return extension_service_worker_voter_.voter_id();
  }

  DummyVoteObserver observer_;
  ExtensionServiceWorkerVoter extension_service_worker_voter_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(ExtensionServiceWorkerVoterTest, AddExtensionServiceWorker) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestWorkerNodeFactory test_worker_node_factory_(graph());

  ProcessNodeImpl* process_node = mock_graph.process.get();
  FrameNodeImpl* frame_node = mock_graph.frame.get();

  EXPECT_EQ(observer_.GetVoteCount(), 0u);

  const auto origin = url::Origin::Create(GURL("chrome-extension://abcd"));
  WorkerNodeImpl* worker_node = test_worker_node_factory_.CreateDedicatedWorker(
      process_node, frame_node, origin);

  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(voter_id(), GetExecutionContext(worker_node),
                                base::TaskPriority::USER_VISIBLE,
                                ExtensionServiceWorkerVoter::kPriorityReason));

  test_worker_node_factory_.DeleteWorker(worker_node);

  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

TEST_F(ExtensionServiceWorkerVoterTest, AddNonExtensionServiceWorker) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestWorkerNodeFactory test_worker_node_factory_(graph());

  ProcessNodeImpl* process_node = mock_graph.process.get();
  FrameNodeImpl* frame_node = mock_graph.frame.get();

  EXPECT_EQ(observer_.GetVoteCount(), 0u);

  const auto origin = url::Origin::Create(GURL("https://example.com"));
  WorkerNodeImpl* worker_node = test_worker_node_factory_.CreateDedicatedWorker(
      process_node, frame_node, origin);

  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(voter_id(), GetExecutionContext(worker_node),
                                base::TaskPriority::LOWEST,
                                ExtensionServiceWorkerVoter::kPriorityReason));

  test_worker_node_factory_.DeleteWorker(worker_node);

  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority
