// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/test/gtest_util.h"
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

// Some dummy vote contexts. These are never dereferenced. They deliberately
// hold different alternatives (FrameNode vs. WorkerNode) so the tests also
// cover keys of different node types.
const VoteContext kVoteContext0 =
    reinterpret_cast<const FrameNode*>(0xDEADBEEF);
const VoteContext kVoteContext1 =
    reinterpret_cast<const WorkerNode*>(0xBAADF00D);

static const Vote kLowPriorityVote0(base::Process::Priority::kMinValue,
                                    "low reason 0");
static const Vote kLowPriorityVote1(base::Process::Priority::kMinValue,
                                    "low reason 1");

static const Vote kMediumPriorityVote0(base::Process::Priority::kUserVisible,
                                       "medium reason 0");
static const Vote kMediumPriorityVote1(base::Process::Priority::kUserVisible,
                                       "medium reason 1");

static const Vote kHighPriorityVote0(base::Process::Priority::kMaxValue,
                                     "high reason 0");
static const Vote kHighPriorityVote1(base::Process::Priority::kMaxValue,
                                     "high reason 1");

// Forwards the top votes of the aggregator to `voting_channel`.
class ForwardingObserver : public MaxVoteAggregator::Observer {
 public:
  explicit ForwardingObserver(VotingChannel voting_channel)
      : voting_channel_(std::move(voting_channel)) {}

  VoterId voter_id() const { return voting_channel_.voter_id(); }

  // MaxVoteAggregator::Observer:
  void OnFrameTopVoteChanged(const FrameNode* frame_node,
                             const std::optional<Vote>& vote) override {
    voting_channel_.SetVote(frame_node, vote);
  }
  void OnWorkerTopVoteChanged(const WorkerNode* worker_node,
                              const std::optional<Vote>& vote) override {
    voting_channel_.SetVote(worker_node, vote);
  }
  void OnPageTopVoteChanged(const PageNode* page_node,
                            const std::optional<Vote>& vote) override {
    voting_channel_.SetVote(page_node, vote);
  }
  void OnProcessTopVoteChanged(const ProcessNode* process_node,
                               const std::optional<Vote>& vote) override {
    voting_channel_.SetVote(process_node, vote);
  }

 private:
  VotingChannel voting_channel_;
};

}  // namespace

class MaxVoteAggregatorTest : public testing::Test {
 public:
  MaxVoteAggregatorTest() = default;
  ~MaxVoteAggregatorTest() override = default;

  void SetUp() override { aggregator_.AddObserver(&forwarding_observer_); }

  void TearDown() override {
    aggregator_.RemoveObserver(&forwarding_observer_);
  }

  VoterId aggregator_voter_id() const {
    return forwarding_observer_.voter_id();
  }

  const DummyVoteObserver& observer() const { return observer_; }

  MaxVoteAggregator* aggregator() { return &aggregator_; }

 private:
  DummyVoteObserver observer_;
  ForwardingObserver forwarding_observer_{observer_.BuildVotingChannel()};
  MaxVoteAggregator aggregator_;
};

// Tests that in the case of a single voter, the vote is simply propagated
// upwards.
TEST_F(MaxVoteAggregatorTest, SingleVoter) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();

  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kVoteContext0));

  voter0.SubmitVote(kVoteContext0, kLowPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kLowPriorityVote0));

  // Change only the reason.
  voter0.ChangeVote(kVoteContext0, kLowPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kLowPriorityVote1));

  // Change the priority.
  voter0.ChangeVote(kVoteContext0, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));

  // Add a vote for a different vote context.
  voter0.SubmitVote(kVoteContext1, kMediumPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext1,
                                 kMediumPriorityVote0));

  voter0.ChangeVote(kVoteContext1, kHighPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext1,
                                 kHighPriorityVote1));

  // Invalidate vote for the first vote context.
  voter0.InvalidateVote(kVoteContext0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kVoteContext0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext1,
                                 kHighPriorityVote1));

  voter0.InvalidateVote(kVoteContext1);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kVoteContext0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kVoteContext0));
}

TEST_F(MaxVoteAggregatorTest, TwoVotersOneContext) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();
  VotingChannel voter1 = aggregator()->GetVotingChannel();

  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kVoteContext0));

  // Submit a first vote to the vote context. Using the 2nd voter to test
  // the stability.
  voter1.SubmitVote(kVoteContext0, kLowPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kLowPriorityVote1));

  // Votes are stable. Voting with the same priority but a different reason will
  // not change the upstream vote.
  voter0.SubmitVote(kVoteContext0, kLowPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kLowPriorityVote1));

  // Change the vote of the first voter to a higher priority. This will modify
  // the upstream.
  voter0.ChangeVote(kVoteContext0, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));

  // Change the vote of the second voter to a higher priority but still lower
  // than the first voter's vote.
  voter1.ChangeVote(kVoteContext0, kMediumPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));

  // Invalidate the top vote. This means the second voter will dictate the new
  // top vote.
  voter0.InvalidateVote(kVoteContext0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kMediumPriorityVote1));

  // Invalidate the vote for the second voter. The upstream vote should also be
  // invalidated.
  voter1.InvalidateVote(kVoteContext0);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kVoteContext0));
}

// A less extensive test than TwoVotersOneContext that sanity checks that votes
// for different contextes are aggregated independently.
TEST_F(MaxVoteAggregatorTest, TwoVotersMultipleContext) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();
  VotingChannel voter1 = aggregator()->GetVotingChannel();

  // Vote for vote context 0, making sure the first voter submits a higher
  // priority vote.
  voter0.SubmitVote(kVoteContext0, kHighPriorityVote0);
  voter1.SubmitVote(kVoteContext0, kMediumPriorityVote1);

  // Vote for vote context 1, making sure the second voter submits a higher
  // priority vote.
  voter0.SubmitVote(kVoteContext1, kLowPriorityVote0);
  voter1.SubmitVote(kVoteContext1, kMediumPriorityVote1);

  // There is an aggregated vote for each context.
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext1,
                                 kMediumPriorityVote1));

  // Cleanup.
  voter0.InvalidateVote(kVoteContext0);
  voter0.InvalidateVote(kVoteContext1);
  voter1.InvalidateVote(kVoteContext0);
  voter1.InvalidateVote(kVoteContext1);

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
    voter.SubmitVote(kVoteContext0, kLowPriorityVote0);

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kLowPriorityVote0));

  // Pick a random voter and change its vote.
  int chosen_voter_index = base::RandGenerator(kNumVoters);
  voters[chosen_voter_index].ChangeVote(kVoteContext0, kHighPriorityVote0);

  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));

  // Cleanup.
  for (auto& voter : voters)
    voter.InvalidateVote(kVoteContext0);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

TEST_F(MaxVoteAggregatorTest, HasVotes) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();
  VotingChannel voter1 = aggregator()->GetVotingChannel();
  EXPECT_FALSE(aggregator()->HasVotes(kVoteContext0));

  voter0.SetVote(kVoteContext0, kLowPriorityVote0);
  voter1.SetVote(kVoteContext0, kHighPriorityVote1);
  EXPECT_TRUE(aggregator()->HasVotes(kVoteContext0));
  EXPECT_FALSE(aggregator()->HasVotes(kVoteContext1));

  // Still has a vote as long as one voter has a vote.
  voter1.SetVote(kVoteContext0, std::nullopt);
  EXPECT_TRUE(aggregator()->HasVotes(kVoteContext0));

  voter0.SetVote(kVoteContext0, std::nullopt);
  EXPECT_FALSE(aggregator()->HasVotes(kVoteContext0));
}

TEST_F(MaxVoteAggregatorTest, GetVote) {
  VotingChannel voter0 = aggregator()->GetVotingChannel();
  VotingChannel voter1 = aggregator()->GetVotingChannel();
  EXPECT_EQ(aggregator()->GetVote(kVoteContext0), std::nullopt);

  voter0.SetVote(kVoteContext0, kLowPriorityVote0);
  EXPECT_EQ(aggregator()->GetVote(kVoteContext0), kLowPriorityVote0);

  voter1.SetVote(kVoteContext0, kHighPriorityVote1);
  EXPECT_EQ(aggregator()->GetVote(kVoteContext0), kHighPriorityVote1);
  EXPECT_EQ(aggregator()->GetVote(kVoteContext1), std::nullopt);

  voter1.SetVote(kVoteContext0, std::nullopt);
  EXPECT_EQ(aggregator()->GetVote(kVoteContext0), kLowPriorityVote0);

  voter0.SetVote(kVoteContext0, std::nullopt);
  EXPECT_EQ(aggregator()->GetVote(kVoteContext0), std::nullopt);
}

namespace {

// Casts `vote_to_cast` on `to_context`, back into the aggregator, while
// `from_frame_node` has a top vote.
class LoopbackObserver : public MaxVoteAggregator::Observer {
 public:
  LoopbackObserver(MaxVoteAggregator* aggregator,
                   const FrameNode* from_frame_node,
                   VoteContext to_context,
                   const Vote& vote_to_cast)
      : aggregator_(aggregator),
        loopback_(aggregator->GetVotingChannel()),
        from_frame_node_(from_frame_node),
        to_context_(to_context),
        vote_to_cast_(vote_to_cast) {
    aggregator_->AddObserver(this);
  }

  ~LoopbackObserver() override { aggregator_->RemoveObserver(this); }

  // MaxVoteAggregator::Observer:
  void OnFrameTopVoteChanged(const FrameNode* frame_node,
                             const std::optional<Vote>& vote) override {
    if (frame_node == from_frame_node_) {
      loopback_.SetVote(*to_context_, vote.has_value()
                                          ? std::make_optional(vote_to_cast_)
                                          : std::nullopt);
    }
  }

 private:
  raw_ptr<MaxVoteAggregator> aggregator_;
  VotingChannel loopback_;
  raw_ptr<const FrameNode> from_frame_node_;
  std::optional<VoteContext> to_context_;
  Vote vote_to_cast_;
};

}  // namespace

// Tests that an observer can re-entrantly cast a vote on another vote context.
TEST_F(MaxVoteAggregatorTest, ReentrantVoteOnOtherContext) {
  LoopbackObserver loopback(aggregator(),
                            std::get<const FrameNode*>(kVoteContext0),
                            kVoteContext1, kMediumPriorityVote1);
  VotingChannel voter0 = aggregator()->GetVotingChannel();

  voter0.SetVote(kVoteContext0, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext0,
                                 kHighPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kVoteContext1,
                                 kMediumPriorityVote1));

  voter0.SetVote(kVoteContext0, std::nullopt);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

// Tests that an observer re-entrantly changing the top vote of the vote context
// being notified is caught.
TEST_F(MaxVoteAggregatorTest, ReentrantVoteOnSameContext) {
  LoopbackObserver loopback(aggregator(),
                            std::get<const FrameNode*>(kVoteContext0),
                            kVoteContext0, kHighPriorityVote1);
  VotingChannel voter0 = aggregator()->GetVotingChannel();

  EXPECT_CHECK_DEATH(voter0.SetVote(kVoteContext0, kLowPriorityVote0));
}

}  // namespace execution_context_priority
}  // namespace performance_manager
