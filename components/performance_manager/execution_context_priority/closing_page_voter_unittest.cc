// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/closing_page_voter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

class ClosingPageVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ClosingPageVoterTest() = default;
  ~ClosingPageVoterTest() override = default;

  ClosingPageVoterTest(const ClosingPageVoterTest&) = delete;
  ClosingPageVoterTest& operator=(const ClosingPageVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    closing_page_voter_.InitializeOnGraph(graph(),
                                          observer_.BuildVotingChannel());
  }

  void TearDown() override {
    closing_page_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  VoterId voter_id() const { return closing_page_voter_.voter_id(); }

  DummyVoteObserver observer_;
  ClosingPageVoter closing_page_voter_;
};

}  // namespace

// Tests that a USER_BLOCKING vote is cast for the entire frame tree when a page
// is closing.
TEST_F(ClosingPageVoterTest, VoteWhenClosingWithChildFrame) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();
  auto* child_frame_node = mock_graph.child_frame.get();

  // No votes initially.
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Set to closing, expect a USER_BLOCKING vote on each frame.
  closing_page_voter_.SetPageIsClosing(page_node, true);
  EXPECT_EQ(observer_.GetVoteCount(), 2u);
  EXPECT_TRUE(observer_.HasVote(voter_id(),
                                GetExecutionContext(main_frame_node),
                                base::TaskPriority::USER_BLOCKING,
                                ClosingPageVoter::kPageIsClosingReason));
  EXPECT_TRUE(observer_.HasVote(voter_id(),
                                GetExecutionContext(child_frame_node),
                                base::TaskPriority::USER_BLOCKING,
                                ClosingPageVoter::kPageIsClosingReason));

  // Set back to not closing, expect the votes to be invalidated.
  closing_page_voter_.SetPageIsClosing(page_node, false);
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(child_frame_node)));
}

// Tests that the vote is invalidated when the page node is removed.
TEST_F(ClosingPageVoterTest, VoteInvalidatedOnRemoval) {
  auto mock_graph =
      std::make_unique<MockSinglePageInSingleProcessGraph>(graph());
  auto* page_node = mock_graph->page.get();
  auto* main_frame_node = mock_graph->frame.get();

  // Set to closing and verify the vote exists.
  closing_page_voter_.SetPageIsClosing(page_node, true);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));

  // Reset the graph, which deletes the nodes. The voter should invalidate its
  // vote in OnBeforePageNodeRemoved.
  mock_graph.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

// Tests that a frame added to a closing page gets a USER_BLOCKING vote.
TEST_F(ClosingPageVoterTest, FrameAddedToClosingPage) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();

  // Set to closing, expect a USER_BLOCKING vote on the main frame.
  closing_page_voter_.SetPageIsClosing(page_node, true);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));

  // Add a child frame, expect a vote on it.
  auto child_frame_node = graph()->CreateFrameNodeAutoId(
      mock_graph.process.get(), page_node, main_frame_node);
  EXPECT_EQ(observer_.GetVoteCount(), 2u);
  EXPECT_TRUE(observer_.HasVote(voter_id(),
                                GetExecutionContext(child_frame_node.get())));

  // Set back to not closing, expect all votes to be invalidated.
  closing_page_voter_.SetPageIsClosing(page_node, false);
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

// Tests that the vote is invalidated when a frame is removed from a closing
// page.
TEST_F(ClosingPageVoterTest, FrameRemovedFromClosingPage) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();
  auto* child_frame_node = mock_graph.child_frame.get();

  // Set to closing, expect a USER_BLOCKING vote on each frame.
  closing_page_voter_.SetPageIsClosing(page_node, true);
  EXPECT_EQ(observer_.GetVoteCount(), 2u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Remove the child frame, its vote should be invalidated.
  mock_graph.child_frame.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));
}

}  // namespace performance_manager::execution_context_priority
