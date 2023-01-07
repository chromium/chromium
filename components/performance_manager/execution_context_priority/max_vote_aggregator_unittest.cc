// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/max_vote_aggregator.h"

#include "base/rand_util.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace execution_context_priority {

// Expose the VoteData type for testing.
class MaxVoteAggregatorTestAccess {
 public:
  using VoteData = MaxVoteAggregator::VoteData;
  using StampedVote = MaxVoteAggregator::StampedVote;
};
using VoteData = MaxVoteAggregatorTestAccess::VoteData;
using StampedVote = MaxVoteAggregatorTestAccess::StampedVote;

namespace {

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

}  // namespace

class MaxVoteAggregatorTest : public testing::Test {
 public:
  MaxVoteAggregatorTest() = default;
  ~MaxVoteAggregatorTest() override = default;

  void SetUp() override {
    VotingChannel channel = observer_.BuildVotingChannel();
    aggregator_voter_id_ = channel.voter_id();
    aggregator_.SetUpstreamVotingChannel(std::move(channel));
  }

  void TearDown() override {}

  VoterId aggregator_voter_id() const { return aggregator_voter_id_; }

  const DummyVoteObserver& observer() const { return observer_; }

  MaxVoteAggregator* aggregator() { return &aggregator_; }

 private:
  DummyVoteObserver observer_;
  MaxVoteAggregator aggregator_;
  VoterId aggregator_voter_id_;
};

// Tests that in the case of a single voter, the vote is simply propagated
// upwards.
TEST_F(MaxVoteAggregatorTest, SingleVoter) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();

  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));

  voter0.SubmitVote(kExecutionContext0, kLowPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kLowPriorityVote0));

  // Change only the reason.
  voter0.ChangeVote(kExecutionContext0, kLowPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kLowPriorityVote1));

  // Change the priority.
  voter0.ChangeVote(kExecutionContext0, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));

  // Add a vote for a different execution context.
  voter0.SubmitVote(kExecutionContext1, kMediumPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote0));

  voter0.ChangeVote(kExecutionContext1, kHighPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kHighPriorityVote1));

  // Invalidate vote for the first execution context.
  voter0.InvalidateVote(kExecutionContext0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kHighPriorityVote1));

  voter0.InvalidateVote(kExecutionContext1);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
}

TEST_F(MaxVoteAggregatorTest, TwoVotersOneContext) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();
  VotingChannel voter1 = aggregator()->GetVotingChannel();

  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));

  // Submit a first vote to the execution context. Using the 2nd voter to test
  // the stability.
  voter1.SubmitVote(kExecutionContext0, kLowPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kLowPriorityVote1));

  // Votes are stable. Voting with the same priority but a different reason will
  // not change the upstream vote.
  voter0.SubmitVote(kExecutionContext0, kLowPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kLowPriorityVote1));

  // Change the vote of the first voter to a higher priority. This will modify
  // the upstream.
  voter0.ChangeVote(kExecutionContext0, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));

  // Change the vote of the second voter to a higher priority but still lower
  // than the first voter's vote.
  voter1.ChangeVote(kExecutionContext0, kMediumPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));

  // Invalidate the top vote. This means the second voter will dictate the new
  // top vote.
  voter0.InvalidateVote(kExecutionContext0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote1));

  // Invalidate the vote for the second voter. The upstream vote should also be
  // invalidated.
  voter1.InvalidateVote(kExecutionContext0);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
}

// A less extensive test than TwoVotersOneContext that sanity checks that votes
// for different contextes are aggregated independently.
TEST_F(MaxVoteAggregatorTest, TwoVotersMultipleContext) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();
  VotingChannel voter1 = aggregator()->GetVotingChannel();

  // Vote for execution context 1, making sure the first voter submits a higher
  // priority vote.
  voter0.SubmitVote(kExecutionContext0, kHighPriorityVote0);
  voter1.SubmitVote(kExecutionContext0, kMediumPriorityVote1);

  // Vote for execution context 2, making sure the second voter submits a higher
  // priority vote.
  voter0.SubmitVote(kExecutionContext1, kLowPriorityVote0);
  voter1.SubmitVote(kExecutionContext1, kMediumPriorityVote1);

  // There is an aggregated vote for each context.
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote1));

  // Cleanup.
  voter0.InvalidateVote(kExecutionContext0);
  voter0.InvalidateVote(kExecutionContext1);
  voter1.InvalidateVote(kExecutionContext0);
  voter1.InvalidateVote(kExecutionContext1);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

// A simple test that ensures MaxVoteAggregator supports an arbitrary number of
// voters.
TEST_F(MaxVoteAggregatorTest, LotsOfVoters) {
  static constexpr int kNumVoters = 2000;
  std::vector<VotingChannel> voters;

  voters.reserve(kNumVoters);
  for (int i = 0; i < kNumVoters; ++i) {
    VotingChannel voter = aggregator()->GetVotingChannel();
    voters.push_back(std::move(voter));
  }

  for (auto& voter : voters)
    voter.SubmitVote(kExecutionContext0, kLowPriorityVote0);

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kLowPriorityVote0));

  // Pick a random voter and change its vote.
  int chosen_voter_index = base::RandGenerator(kNumVoters);
  voters[chosen_voter_index].ChangeVote(kExecutionContext0, kHighPriorityVote0);

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));

  // Cleanup.
  for (auto& voter : voters)
    voter.InvalidateVote(kExecutionContext0);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace execution_context_priority
}  // namespace performance_manager
