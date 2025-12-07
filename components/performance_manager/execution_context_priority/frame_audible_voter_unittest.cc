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
    frame_audible_voter_.InitializeOnGraph(graph(),
                                           observer_.BuildVotingChannel());
  }

  void TearDown() override {
    frame_audible_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return frame_audible_voter_.voter_id(); }

 private:
  DummyVoteObserver observer_;
  FrameAudibleVoter frame_audible_voter_;
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
