// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/inherit_client_priority_voter.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/execution_context/execution_context_registry_impl.h"
#include "components/performance_manager/execution_context_priority/root_vote_observer.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/test_worker_node_factory.h"
#include "components/performance_manager/test_support/voting.h"

namespace performance_manager {
namespace execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const WorkerNode* worker_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             worker_node->GetGraph())
      ->GetExecutionContextForWorkerNode(worker_node);
}

// All voting system components are expected to live on the graph, without being
// actual GraphOwned objects. This class wraps them to allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper() {
    VotingChannel voting_channel = dummy_vote_observer_.BuildVotingChannel();
    voter_id_ = voting_channel.voter_id();
    inherit_client_priority_voter_.SetVotingChannel(std::move(voting_channel));
  }

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddFrameNodeObserver(&inherit_client_priority_voter_);
    graph->AddWorkerNodeObserver(&inherit_client_priority_voter_);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveWorkerNodeObserver(&inherit_client_priority_voter_);
    graph->RemoveFrameNodeObserver(&inherit_client_priority_voter_);
  }

  // Exposes the vote observer to validate expectations.
  const DummyVoteObserver& observer() const { return dummy_vote_observer_; }

  VoterId voter_id() { return voter_id_; }

 private:
  DummyVoteObserver dummy_vote_observer_;

  InheritClientPriorityVoter inherit_client_priority_voter_;

  // The VoterId of |inherit_client_priority_voter_|.
  VoterId voter_id_;
};

}  // namespace

class InheritClientPriorityVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  void SetUp() override {
    Super::SetUp();

    graph()->PassToGraph(
        std::make_unique<execution_context::ExecutionContextRegistryImpl>());

    auto wrapper = std::make_unique<GraphOwnedWrapper>();
    wrapper_ = wrapper.get();
    graph()->PassToGraph(std::move(wrapper));
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return wrapper_->observer(); }

  VoterId voter_id() { return wrapper_->voter_id(); }

 private:
  raw_ptr<GraphOwnedWrapper> wrapper_ = nullptr;
};

TEST_F(InheritClientPriorityVoterTest, OneWorker) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestWorkerNodeFactory test_worker_node_factory_(graph());

  ProcessNodeImpl* process_node = mock_graph.process.get();
  FrameNodeImpl* frame_node = mock_graph.frame.get();

  EXPECT_EQ(observer().GetVoteCount(), 0u);

  // Create the worker. A vote will be submitted to inherit the (currently
  // default) priority of its client.
  WorkerNodeImpl* worker_node =
      test_worker_node_factory_.CreateDedicatedWorker(process_node, frame_node);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(worker_node), base::TaskPriority::LOWEST,
      InheritClientPriorityVoter::kPriorityInheritedReason));

  // Test that changing the priority for the frame also changes the inherited
  // vote.
  frame_node->SetPriorityAndReason(
      {base::TaskPriority::USER_VISIBLE, "Some reason"});

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(worker_node),
                         base::TaskPriority::USER_VISIBLE,
                         InheritClientPriorityVoter::kPriorityInheritedReason));

  // Removing the worker also removes the inherited vote.
  test_worker_node_factory_.DeleteWorker(worker_node);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

// A vote is submitted to all children.
TEST_F(InheritClientPriorityVoterTest, MultipleWorkers) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestWorkerNodeFactory test_worker_node_factory_(graph());

  ProcessNodeImpl* process_node = mock_graph.process.get();
  FrameNodeImpl* frame_node = mock_graph.frame.get();

  // Set the priority of the frame node.
  frame_node->SetPriorityAndReason(
      {base::TaskPriority::USER_VISIBLE, "Some reason"});

  EXPECT_EQ(observer().GetVoteCount(), 0u);

  // Create multiple workers with the same client.
  WorkerNodeImpl* worker_node_1 =
      test_worker_node_factory_.CreateDedicatedWorker(process_node, frame_node);
  WorkerNodeImpl* worker_node_2 =
      test_worker_node_factory_.CreateDedicatedWorker(process_node, frame_node);

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(worker_node_1),
                         base::TaskPriority::USER_VISIBLE,
                         InheritClientPriorityVoter::kPriorityInheritedReason));
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(worker_node_2),
                         base::TaskPriority::USER_VISIBLE,
                         InheritClientPriorityVoter::kPriorityInheritedReason));
}

// Tests that the priority is recursively inherited down a worker tree.
TEST_F(InheritClientPriorityVoterTest, DeepWorkerTree) {
  constexpr size_t kTreeDepth = 20;

  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestWorkerNodeFactory test_worker_node_factory_(graph());

  ProcessNodeImpl* process_node = mock_graph.process.get();
  FrameNodeImpl* frame_node = mock_graph.frame.get();

  // Set the priority of the frame node.
  frame_node->SetPriorityAndReason(
      {base::TaskPriority::USER_VISIBLE, "Some reason"});

  std::vector<WorkerNodeImpl*> worker_nodes;
  worker_nodes.reserve(kTreeDepth);

  // Create the first worker. Its client is the only frame.
  worker_nodes.push_back(test_worker_node_factory_.CreateDedicatedWorker(
      process_node, frame_node));

  // The ExecutionContextPriorityDecorator is not hooked up in this test suite
  // so the priority is not actually inherited by the node. Set the priority
  // manually to at least ensure that the vote contains a non-default value.
  worker_nodes.back()->SetPriorityAndReason(
      {base::TaskPriority::USER_VISIBLE, "Some reason"});

  // Create the other workers, where each of them is the child of the last
  // created worker.
  for (size_t i = 0; i < kTreeDepth - 1; i++) {
    worker_nodes.push_back(test_worker_node_factory_.CreateDedicatedWorker(
        process_node, worker_nodes.back()));
    worker_nodes.back()->SetPriorityAndReason(
        {base::TaskPriority::USER_VISIBLE, "Some reason"});
  }

  EXPECT_EQ(observer().GetVoteCount(), kTreeDepth);
  for (WorkerNodeImpl* worker_node : worker_nodes) {
    ASSERT_TRUE(observer().HasVote(
        voter_id(), GetExecutionContext(worker_node),
        base::TaskPriority::USER_VISIBLE,
        InheritClientPriorityVoter::kPriorityInheritedReason));
  }
}

// Each client contributes a vote to a worker. Those votes are aggregated to a
// single vote.
TEST_F(InheritClientPriorityVoterTest, MultipleClients) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  TestWorkerNodeFactory test_worker_node_factory_(graph());

  ProcessNodeImpl* process_node = mock_graph.process.get();
  FrameNodeImpl* frame_node_1 = mock_graph.frame.get();
  FrameNodeImpl* frame_node_2 = mock_graph.child_frame.get();

  EXPECT_EQ(observer().GetVoteCount(), 0u);

  // Create a worker with multiple clients.
  WorkerNodeImpl* worker_node = test_worker_node_factory_.CreateSharedWorker(
      process_node, {frame_node_1, frame_node_2});

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(worker_node), base::TaskPriority::LOWEST,
      InheritClientPriorityVoter::kPriorityInheritedReason));

  // Change the priority of the first client.
  frame_node_1->SetPriorityAndReason(
      {base::TaskPriority::USER_VISIBLE, "Some reason"});
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(worker_node),
                         base::TaskPriority::USER_VISIBLE,
                         InheritClientPriorityVoter::kPriorityInheritedReason));

  // Change the priority of the second client.
  frame_node_2->SetPriorityAndReason(
      {base::TaskPriority::USER_BLOCKING, "Some reason"});
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(worker_node),
                         base::TaskPriority::USER_BLOCKING,
                         InheritClientPriorityVoter::kPriorityInheritedReason));
}

TEST_F(InheritClientPriorityVoterTest, SamePriorityDifferentReason) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestWorkerNodeFactory test_worker_node_factory_(graph());

  ProcessNodeImpl* process_node = mock_graph.process.get();
  FrameNodeImpl* frame_node = mock_graph.frame.get();

  EXPECT_EQ(observer().GetVoteCount(), 0u);

  // Create the worker. A vote will be submitted to inherit the (currently
  // default) priority of its client.
  WorkerNodeImpl* worker_node =
      test_worker_node_factory_.CreateDedicatedWorker(process_node, frame_node);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(worker_node), base::TaskPriority::LOWEST,
      InheritClientPriorityVoter::kPriorityInheritedReason));

  // Set a different PriorityAndReason. The priority stays the same, but the
  // reason changed.
  frame_node->SetPriorityAndReason({base::TaskPriority::LOWEST, "Some reason"});

  // Should not change the inherited priority and should not crash.
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(worker_node), base::TaskPriority::LOWEST,
      InheritClientPriorityVoter::kPriorityInheritedReason));

  // Removing the worker also removes the inherited vote.
  test_worker_node_factory_.DeleteWorker(worker_node);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
