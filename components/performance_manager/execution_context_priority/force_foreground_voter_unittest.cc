// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/force_foreground_voter.h"

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

class ForceForegroundVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ForceForegroundVoterTest() = default;
  ~ForceForegroundVoterTest() override = default;

  ForceForegroundVoterTest(const ForceForegroundVoterTest&) = delete;
  ForceForegroundVoterTest& operator=(const ForceForegroundVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    voter_.InitializeOnGraph(graph(), observer_.BuildVotingChannel());
  }

  void TearDown() override {
    voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  VoterId voter_id() const { return voter_.voter_id(); }

  DummyVoteObserver observer_;
  ForceForegroundVoter voter_;
};

}  // namespace

// Tests that a USER_BLOCKING vote is cast for all frames (main and subframes)
// and workers.
TEST_F(ForceForegroundVoterTest, VoteFramesAndWorkers) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  // Expect a USER_BLOCKING vote on each frame.
  EXPECT_EQ(observer_.GetVoteCount(), 5u);
  EXPECT_TRUE(observer_.HasVote(
      voter_id(),
      execution_context::ExecutionContext::From(mock_graph.frame.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoter::kForceForegroundReason));
  EXPECT_TRUE(observer_.HasVote(
      voter_id(),
      execution_context::ExecutionContext::From(mock_graph.child_frame.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoter::kForceForegroundReason));
  EXPECT_TRUE(observer_.HasVote(
      voter_id(),
      execution_context::ExecutionContext::From(mock_graph.worker.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoter::kForceForegroundReason));
}

}  // namespace performance_manager::execution_context_priority
