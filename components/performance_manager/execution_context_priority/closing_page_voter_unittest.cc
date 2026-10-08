// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/closing_page_voter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

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

// Tests that a USER_BLOCKING vote is cast for the page when it is closing.
TEST_F(ClosingPageVoterTest, VoteWhenClosing) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // No votes initially.
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  EXPECT_FALSE(observer_.HasVote(voter_id(), page_node));

  // Set to closing, expect a USER_BLOCKING vote on the page.
  closing_page_voter_.SetPageIsClosing(page_node, true);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(voter_id(), page_node,
                                base::Process::Priority::kUserBlocking,
                                ClosingPageVoter::kPageIsClosingReason));

  // Set back to not closing, expect the vote to be invalidated.
  closing_page_voter_.SetPageIsClosing(page_node, false);
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  EXPECT_FALSE(observer_.HasVote(voter_id(), page_node));
}

// Tests that the vote is invalidated when the page node is removed.
TEST_F(ClosingPageVoterTest, VoteInvalidatedOnRemoval) {
  auto mock_graph =
      std::make_unique<MockSinglePageInSingleProcessGraph>(graph());
  auto* page_node = mock_graph->page.get();

  // Set to closing and verify the vote exists.
  closing_page_voter_.SetPageIsClosing(page_node, true);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(voter_id(), page_node));

  // Reset the graph, which deletes the nodes. The voter should invalidate its
  // vote in OnBeforePageNodeRemoved.
  mock_graph.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority
