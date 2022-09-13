// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/override_vote_aggregator.h"

#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

// Some dummy execution contexts.
const ExecutionContext* kExecutionContext0 =
    reinterpret_cast<const ExecutionContext*>(0xDEADBEEF);
const ExecutionContext* kExecutionContext1 =
    reinterpret_cast<const ExecutionContext*>(0xBAADF00D);

static const Vote kLowPriorityVote0(base::TaskPriority::LOWEST, "low reason 0");
static const Vote kLowPriorityVote1(base::TaskPriority::LOWEST, "low reason 1");

static const Vote kMediumPriorityVote0(base::TaskPriority::USER_VISIBLE,
                                       "medium reason 0");
static const Vote kMediumPriorityVote1(base::TaskPriority::USER_VISIBLE,
                                       "medium reason 1");

static const Vote kHighPriorityVote0(base::TaskPriority::HIGHEST,
                                     "high reason 0");
static const Vote kHighPriorityVote1(base::TaskPriority::HIGHEST,
                                     "high reason 1");

class OverrideVoteAggregatorTest : public testing::Test {
 public:
  OverrideVoteAggregatorTest() = default;
  ~OverrideVoteAggregatorTest() override = default;

  void SetUp() override {
    VotingChannel channel = observer_.BuildVotingChannel();
    aggregator_voter_id_ = channel.voter_id();
    aggregator_.SetUpstreamVotingChannel(std::move(channel));
  }

  void TearDown() override {}

  VoterId aggregator_voter_id() const { return aggregator_voter_id_; }

  const DummyVoteObserver& observer() const { return observer_; }

  OverrideVoteAggregator* aggregator() { return &aggregator_; }

  void TestSingleVoter(VotingChannel* voter) {
    EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));

    voter->SubmitVote(kExecutionContext0, kLowPriorityVote0);
    EXPECT_EQ(observer().GetVoteCount(), 1u);
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                   kLowPriorityVote0));

    // Change only the reason.
    voter->ChangeVote(kExecutionContext0, kLowPriorityVote1);
    EXPECT_EQ(observer().GetVoteCount(), 1u);
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                   kLowPriorityVote1));

    // Change the priority.
    voter->ChangeVote(kExecutionContext0, kHighPriorityVote0);
    EXPECT_EQ(observer().GetVoteCount(), 1u);
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                   kHighPriorityVote0));

    // Add a vote for a different execution context.
    voter->SubmitVote(kExecutionContext1, kMediumPriorityVote0);
    EXPECT_EQ(observer().GetVoteCount(), 2u);
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                   kHighPriorityVote0));
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                   kMediumPriorityVote0));

    voter->ChangeVote(kExecutionContext1, kHighPriorityVote1);
    EXPECT_EQ(observer().GetVoteCount(), 2u);
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                   kHighPriorityVote0));
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                   kHighPriorityVote1));

    // Invalidate vote for the first execution context.
    voter->InvalidateVote(kExecutionContext0);
    EXPECT_EQ(observer().GetVoteCount(), 1u);
    EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
    EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                   kHighPriorityVote1));

    voter->InvalidateVote(kExecutionContext1);
    EXPECT_EQ(observer().GetVoteCount(), 0u);
    EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
    EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
  }

 private:
  DummyVoteObserver observer_;
  OverrideVoteAggregator aggregator_;
  VoterId aggregator_voter_id_;
};

// Tests that in the case of a single voter, the vote is simply propagated
// upwards.
TEST_F(OverrideVoteAggregatorTest, SingleVoter) {
  VotingChannel default_voter = aggregator()->GetDefaultVotingChannel();
  VotingChannel override_voter = aggregator()->GetOverrideVotingChannel();

  TestSingleVoter(&default_voter);
  TestSingleVoter(&override_voter);
}

TEST_F(OverrideVoteAggregatorTest, OneContext) {
  VotingChannel default_voter = aggregator()->GetDefaultVotingChannel();
  VotingChannel override_voter = aggregator()->GetOverrideVotingChannel();

  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));

  // Submit a default vote for the execution context.
  default_voter.SubmitVote(kExecutionContext0, kMediumPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));

  // Submit an override vote. The override vote will always be upstreamed,
  // regardless of the priority.
  override_voter.SubmitVote(kExecutionContext0, kLowPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kLowPriorityVote1));

  // Change the override vote. The upstream will also be changed.
  override_voter.ChangeVote(kExecutionContext0, kHighPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote1));

  // Change the default vote. The upstream will not change, but the default vote
  // will be remembered.
  default_voter.ChangeVote(kExecutionContext0, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote1));

  // Invalidate the override vote. The upstream will change to the default vote.
  override_voter.InvalidateVote(kExecutionContext0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));

  // Cleanup.
  default_voter.InvalidateVote(kExecutionContext0);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
}

// A less extensive test than OneContext that sanity checks that votes for
// different contexts are aggregated independently.
TEST_F(OverrideVoteAggregatorTest, MultipleContexts) {
  VotingChannel default_voter = aggregator()->GetDefaultVotingChannel();
  VotingChannel override_voter = aggregator()->GetOverrideVotingChannel();

  // Vote for execution context 1. The override vote lowers the priority of the
  // upstreamed vote.
  default_voter.SubmitVote(kExecutionContext0, kHighPriorityVote0);
  override_voter.SubmitVote(kExecutionContext0, kMediumPriorityVote1);

  // Vote for execution context 2. The override vote increases the priority of
  // the upstreamed vote.
  default_voter.SubmitVote(kExecutionContext1, kLowPriorityVote0);
  override_voter.SubmitVote(kExecutionContext1, kHighPriorityVote1);

  // There is an aggregated vote for each context, and their values are coming
  // from the override voter.
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote1));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kHighPriorityVote1));

  // Cleanup.
  default_voter.InvalidateVote(kExecutionContext0);
  default_voter.InvalidateVote(kExecutionContext1);
  override_voter.InvalidateVote(kExecutionContext0);
  override_voter.InvalidateVote(kExecutionContext1);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
