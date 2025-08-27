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

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return closing_page_voter_.voter_id(); }

 private:
  DummyVoteObserver observer_;
  ClosingPageVoter closing_page_voter_;
};

}  // namespace

// Tests that a USER_BLOCKING vote is cast when a page is closing and
// invalidated when it is no longer closing.
TEST_F(ClosingPageVoterTest, VoteWhenClosing) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();

  // No vote initially.
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(main_frame_node)));

  // Set to closing, expect a USER_BLOCKING vote.
  page_node->SetIsClosing(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(main_frame_node),
                                 base::TaskPriority::USER_BLOCKING,
                                 ClosingPageVoter::kPageIsClosingReason));

  // Set back to not closing, expect the vote to be invalidated.
  page_node->SetIsClosing(false);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(main_frame_node)));
}

// Tests that the vote is invalidated when the page node is removed.
TEST_F(ClosingPageVoterTest, VoteInvalidatedOnRemoval) {
  auto mock_graph =
      std::make_unique<MockSinglePageInSingleProcessGraph>(graph());
  auto* page_node = mock_graph->page.get();
  auto* main_frame_node = mock_graph->frame.get();

  // Set to closing and verify the vote exists.
  page_node->SetIsClosing(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(main_frame_node)));

  // Reset the graph, which deletes the nodes. The voter should invalidate its
  // vote in OnBeforePageNodeRemoved.
  mock_graph.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority
