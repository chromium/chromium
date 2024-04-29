// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/child_frame_booster.h"

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

// The BoostingVoteAggregator, the ChildFrameBooster and the voting channel are
// all expected live on the graph, without being actual GraphOwned objects. This
// class wraps them to allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper() : child_frame_booster_(&boosting_vote_aggregator_) {
    VotingChannel observer_voting_channel = observer_.BuildVotingChannel();
    voter_id_ = observer_voting_channel.voter_id();
    boosting_vote_aggregator_.SetUpstreamVotingChannel(
        std::move(observer_voting_channel));
    voting_channel_ = boosting_vote_aggregator_.GetVotingChannel();
  }

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddInitializingFrameNodeObserver(&child_frame_booster_);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveInitializingFrameNodeObserver(&child_frame_booster_);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

  VotingChannel& voting_channel() { return voting_channel_; }

 private:
  DummyVoteObserver observer_;
  BoostingVoteAggregator boosting_vote_aggregator_;
  ChildFrameBooster child_frame_booster_;
  VoterId voter_id_;
  VotingChannel voting_channel_;
};

static const char kDummyReason[] = "reason";

}  // namespace

class ChildFrameBoosterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ChildFrameBoosterTest() = default;
  ~ChildFrameBoosterTest() override = default;

  ChildFrameBoosterTest(const ChildFrameBoosterTest&) = delete;
  ChildFrameBoosterTest& operator=(const ChildFrameBoosterTest&) = delete;

  void SetUp() override {
    Super::GetGraphFeatures().EnableExecutionContextRegistry();
    Super::SetUp();
    wrapper_ = graph()->PassToGraph(std::make_unique<GraphOwnedWrapper>());
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return wrapper_->observer(); }

  VoterId voter_id() const { return wrapper_->voter_id(); }

  VotingChannel& voting_channel() { return wrapper_->voting_channel(); }

 private:
  raw_ptr<GraphOwnedWrapper> wrapper_ = nullptr;
};

// Tests that the ChildFrameBooster correctly boosts the priority of a child
// frame.
TEST_F(ChildFrameBoosterTest, BoostingWorks) {
  // Create a graph with a parent frame and a child frame. No vote is cast
  // initially.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  const FrameNode* parent_frame_node = mock_graph.frame.get();
  const FrameNode* child_frame_node = mock_graph.child_frame.get();

  // No vote exist initially.
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(parent_frame_node)));
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Cast a vote for the child frame with moderate priority. It won't affect the
  // parent's priority.
  voting_channel().SubmitVote(
      GetExecutionContext(child_frame_node),
      Vote(base::TaskPriority::USER_VISIBLE, kDummyReason));

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(parent_frame_node)));
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node),
                         base::TaskPriority::USER_VISIBLE, kDummyReason));

  // Cast a vote for the parent frame with a priority higher than the children.
  // The child frame will be boosted.
  voting_channel().SubmitVote(
      GetExecutionContext(parent_frame_node),
      Vote(base::TaskPriority::USER_BLOCKING, kDummyReason));

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(parent_frame_node),
                         base::TaskPriority::USER_BLOCKING, kDummyReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node),
                                 base::TaskPriority::USER_BLOCKING,
                                 ChildFrameBooster::kChildFrameBoostReason));

  // Clean-up.
  voting_channel().InvalidateVote(GetExecutionContext(parent_frame_node));
  voting_channel().InvalidateVote(GetExecutionContext(child_frame_node));
}

TEST_F(ChildFrameBoosterTest, AdFrame) {
  // Create a graph with a parent frame and a child frame. No vote is cast
  // initially.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TestNodeWrapper<FrameNodeImpl> child_frame = graph()->CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(), mock_graph.frame.get());
  FrameNodeImpl* parent_frame_node = mock_graph.frame.get();
  FrameNodeImpl* child_frame_node = child_frame.get();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(parent_frame_node)));
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Cast a vote for the parent frame with a high priority. The child frame will
  // be boosted because it is *not* an ad frame initially.
  voting_channel().SubmitVote(
      GetExecutionContext(parent_frame_node),
      Vote(base::TaskPriority::USER_BLOCKING, kDummyReason));

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(parent_frame_node),
                         base::TaskPriority::USER_BLOCKING, kDummyReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node),
                                 base::TaskPriority::USER_BLOCKING,
                                 ChildFrameBooster::kChildFrameBoostReason));

  // Set the ad frame bit on the child. This will remove the boost and thus its
  // vote.
  child_frame_node->SetIsAdFrame(true);

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(parent_frame_node),
                         base::TaskPriority::USER_BLOCKING, kDummyReason));
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Clean-up.
  voting_channel().InvalidateVote(GetExecutionContext(parent_frame_node));
}

}  // namespace performance_manager::execution_context_priority
