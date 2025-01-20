// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/features.h"
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

class FrameVisibilityVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FrameVisibilityVoterTest() = default;
  ~FrameVisibilityVoterTest() override = default;

  FrameVisibilityVoterTest(const FrameVisibilityVoterTest&) = delete;
  FrameVisibilityVoterTest& operator=(const FrameVisibilityVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    frame_visibility_voter_.InitializeOnGraph(graph(),
                                              observer_.BuildVotingChannel());
  }

  void TearDown() override {
    frame_visibility_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return frame_visibility_voter_.voter_id(); }

 private:
  DummyVoteObserver observer_;
  FrameVisibilityVoter frame_visibility_voter_;
};

// Tests that the FrameVisibilityVoter correctly casts a vote for a frame
// depending on its visibility.
TEST_F(FrameVisibilityVoterTest, ChangeFrameVisibility) {
  // Create a graph with a single frame. Its initial visibility should be
  // kUnknown, resulting in a high priority.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_BLOCKING,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Make the frame not visible. This should lower the priority.
  frame_node->SetVisibility(FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::BEST_EFFORT,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Make the frame visible. This should increase the priority.
  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_BLOCKING,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Deleting the frame should invalidate the vote.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

// Tests that the FrameVisibilityVoter correctly casts a USER_VISIBLE vote for a
// unimportant frame that is visible.
TEST_F(FrameVisibilityVoterTest, UnimportantFrames) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kUnimportantFramesPriority);

  // Create a graph with a child frame as only child frames can be unimportant.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  auto& frame_node = mock_graph.child_frame;

  // Make the frame visible and unimportant.
  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  frame_node->SetIsImportant(false);

  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 FrameVisibilityVoter::kFrameVisibilityReason));
}

}  // namespace execution_context_priority
}  // namespace performance_manager
