// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/ad_frame_voter.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"

namespace performance_manager {
namespace execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

// Both the voting channel and the AdFrameVoter are expected live on the graph,
// without being actual GraphOwned objects. This class wraps both to allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper()
      : ad_frame_voter_(observer_.BuildVotingChannel()),
        voter_id_(ad_frame_voter_.voter_id()) {}

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddInitializingFrameNodeObserver(&ad_frame_voter_);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveInitializingFrameNodeObserver(&ad_frame_voter_);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

 private:
  DummyVoteObserver observer_;
  AdFrameVoter ad_frame_voter_;
  VoterId voter_id_;
};

}  // namespace

class AdFrameVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  AdFrameVoterTest() = default;
  ~AdFrameVoterTest() override = default;

  AdFrameVoterTest(const AdFrameVoterTest&) = delete;
  AdFrameVoterTest& operator=(const AdFrameVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    auto wrapper = std::make_unique<GraphOwnedWrapper>();
    wrapper_ = wrapper.get();
    graph()->PassToGraph(std::move(wrapper));
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return wrapper_->observer(); }

  VoterId voter_id() const { return wrapper_->voter_id(); }

 private:
  raw_ptr<GraphOwnedWrapper> wrapper_ = nullptr;
};

// Tests that the AdFrameVoter correctly casts a vote for an ad frame.
TEST_F(AdFrameVoterTest, SetIsAdFrameTrue) {
  // Create a graph with a single frame. It should not initially be an ad frame.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  EXPECT_FALSE(frame_node->IsAdFrame());
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame_node.get())));

  // Make the frame an ad frame. This should cast a low priority vote.
  mock_graph.frame->SetIsAdFrame(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(frame_node.get()),
      base::TaskPriority::LOWEST, AdFrameVoter::kAdFrameReason));

  // Deleting the frame should invalidate the vote.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

// Tests that the AdFrameVoter correctly does not cast a vote for an untagged
// frame.
TEST_F(AdFrameVoterTest, SetIsAdFrameFalse) {
  // Create a graph with a single ad frame.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  mock_graph.frame->SetIsAdFrame(true);
  EXPECT_TRUE(frame_node->IsAdFrame());

  // Unset the frame as an ad. This should invalidate any vote.
  mock_graph.frame->SetIsAdFrame(false);
  EXPECT_FALSE(frame_node->IsAdFrame());
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame_node.get())));

  // Deleting the frame should not affect the vote count.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
