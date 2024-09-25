// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/inherit_parent_priority_voter.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

// The BoostingVoteAggregator, the InheritParentPriorityVoter and the voting
// channel are all expected live on the graph, without being actual GraphOwned
// objects. This class wraps them to allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper()
      : inherit_parent_priority_voter_(observer_.BuildVotingChannel()),
        voter_id_(inherit_parent_priority_voter_.voter_id()) {}

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddInitializingFrameNodeObserver(&inherit_parent_priority_voter_);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveInitializingFrameNodeObserver(&inherit_parent_priority_voter_);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

 private:
  DummyVoteObserver observer_;
  InheritParentPriorityVoter inherit_parent_priority_voter_;
  VoterId voter_id_;
};

static const char kDummyReason[] = "reason";

}  // namespace

class InheritParentPriorityVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  InheritParentPriorityVoterTest() = default;
  ~InheritParentPriorityVoterTest() override = default;

  InheritParentPriorityVoterTest(const InheritParentPriorityVoterTest&) =
      delete;
  InheritParentPriorityVoterTest& operator=(
      const InheritParentPriorityVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    wrapper_ = graph()->PassToGraph(std::make_unique<GraphOwnedWrapper>());
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return wrapper_->observer(); }

  VoterId voter_id() const { return wrapper_->voter_id(); }

 private:
  raw_ptr<GraphOwnedWrapper> wrapper_ = nullptr;
};

// Tests that the InheritParentPriorityVoter correctly casts a vote to each
// child of a frame.
TEST_F(InheritParentPriorityVoterTest, ChildFrame) {
  // Create a graph with a parent frame and a child frame.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  FrameNodeImpl* parent_frame_node = mock_graph.frame.get();
  FrameNodeImpl* child_frame_node = mock_graph.child_frame.get();

  // No vote exist initially.
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Set the parent frame to the USER_VISIBLE priority. The child frame will
  // inherit it through a vote of the same priority.
  parent_frame_node->SetPriorityAndReason(
      {base::TaskPriority::USER_VISIBLE, kDummyReason});

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node),
                         base::TaskPriority::USER_VISIBLE,
                         InheritParentPriorityVoter::kPriorityInheritedReason));

  // Set the parent frame to the USER_BLOCKING priority. The child frame will
  // not inherit a higher priority.
  parent_frame_node->SetPriorityAndReason(
      {base::TaskPriority::USER_BLOCKING, kDummyReason});

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node),
                         base::TaskPriority::USER_VISIBLE,
                         InheritParentPriorityVoter::kPriorityInheritedReason));

  // Set the parent frame to its default value. The existing vote will be
  // invalidated.
  parent_frame_node->SetPriorityAndReason(
      {base::TaskPriority::BEST_EFFORT, kDummyReason});

  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node)));
}

TEST_F(InheritParentPriorityVoterTest, AdFrame) {
  // Create a graph with a parent frame and a child ad frame, which itself has a
  // child.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestNodeWrapper<FrameNodeImpl> child_frame = graph()->CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(), mock_graph.frame.get());
  FrameNodeImpl* parent_frame_node = mock_graph.frame.get();
  FrameNodeImpl* child_frame_node = child_frame.get();

  // No votes exist initially.
  EXPECT_FALSE(child_frame_node->IsAdFrame());
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Set the parent frame to the USER_VISIBLE priority. The child frame will
  // inherit it because it is *not* an ad frame initially.
  parent_frame_node->SetPriorityAndReason(
      {base::TaskPriority::USER_VISIBLE, kDummyReason});

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node),
                         base::TaskPriority::USER_VISIBLE,
                         InheritParentPriorityVoter::kPriorityInheritedReason));

  // Set the ad frame bit. This will remove the vote on the child.
  child_frame_node->SetIsAdFrame(true);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node)));
}

}  // namespace performance_manager::execution_context_priority
