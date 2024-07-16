// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_audible_voter.h"

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

// Both the voting channel and the FrameAudibleVoter are expected live on the
// graph, without being actual GraphOwned objects. This class wraps both to
// allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper()
      : frame_audible_voter_(observer_.BuildVotingChannel()),
        voter_id_(frame_audible_voter_.voter_id()) {}

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    frame_audible_voter_.InitializeOnGraph(graph);
  }
  void OnTakenFromGraph(Graph* graph) override {
    frame_audible_voter_.TearDownOnGraph(graph);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

 private:
  DummyVoteObserver observer_;
  FrameAudibleVoter frame_audible_voter_;
  VoterId voter_id_;
};

}  // namespace

class FrameAudibleVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FrameAudibleVoterTest() = default;
  ~FrameAudibleVoterTest() override = default;

  FrameAudibleVoterTest(const FrameAudibleVoterTest&) = delete;
  FrameAudibleVoterTest& operator=(const FrameAudibleVoterTest&) = delete;

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

// Tests that the FrameAudibleVoter correctly casts a vote for a frame
// depending on its audible state.
TEST_F(FrameAudibleVoterTest, AudibleChanged) {
  // Create a graph with a single frame page. Its initial audible state should
  // be false, resulting in a low priority.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  EXPECT_FALSE(frame_node->IsAudible());
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(frame_node.get()),
      base::TaskPriority::LOWEST, FrameAudibleVoter::kFrameAudibleReason));

  // Make the frame audible. This should increase the priority.
  mock_graph.frame->SetIsAudible(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_BLOCKING,
                                 FrameAudibleVoter::kFrameAudibleReason));

  // Deleting the frame should invalidate the vote.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
