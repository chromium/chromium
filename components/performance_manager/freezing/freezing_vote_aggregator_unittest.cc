// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_vote_aggregator.h"

#include "base/rand_util.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace freezing {

// Expose the FreezingVoteData type for testing.
class FreezingVoteAggregatorTestAccess {
 public:
  using FreezingVoteData = FreezingVoteAggregator::FreezingVoteData;

  static const FreezingVoteData::VotesDeque& GetAllVotes(
      FreezingVoteAggregator* agg,
      const PageNode* node) {
    return agg->GetVoteData(node)->second.GetVotesForTesting();
  }
};
using VoteData = FreezingVoteAggregatorTestAccess::FreezingVoteData;

namespace {

using DummyFreezingVoteObserver = voting::test::DummyVoteObserver<FreezingVote>;

// Some dummy page nodes.
const PageNode* kPageNode0 = reinterpret_cast<const PageNode*>(0xDEADBEEF);
const PageNode* kPageNode1 = reinterpret_cast<const PageNode*>(0xBAADF00D);

static const FreezingVote kCannotFreezeVote0(FreezingVoteValue::kCannotFreeze,
                                             "cannot freeze 0");
static const FreezingVote kCannotFreezeVote1(FreezingVoteValue::kCannotFreeze,
                                             "cannot freeze 1");
static const FreezingVote kCannotFreezeVote2(FreezingVoteValue::kCannotFreeze,
                                             "cannot freeze 2");
static const FreezingVote kCanFreezeVote0(FreezingVoteValue::kCanFreeze,
                                          "can freeze 0");
static const FreezingVote kCanFreezeVote1(FreezingVoteValue::kCanFreeze,
                                          "can freeze 1");
static const FreezingVote kCanFreezeVote2(FreezingVoteValue::kCanFreeze,
                                          "can freeze 2");
static const FreezingVote kCanFreezeVote3(FreezingVoteValue::kCanFreeze,
                                          "can freeze 3");

}  // namespace

class FreezingVoteAggregatorTest : public testing::Test {
 public:
  FreezingVoteAggregatorTest() = default;
  ~FreezingVoteAggregatorTest() override = default;

  void SetUp() override {
    FreezingVotingChannel channel = observer_.BuildVotingChannel();
    aggregator_voter_id_ = channel.voter_id();
    aggregator_.SetUpstreamVotingChannel(std::move(channel));
  }

  void TearDown() override {}

  FreezingVoterId aggregator_voter_id() const { return aggregator_voter_id_; }

  const DummyFreezingVoteObserver& observer() const { return observer_; }

  FreezingVoteAggregator* aggregator() { return &aggregator_; }

 private:
  DummyFreezingVoteObserver observer_;
  FreezingVoteAggregator aggregator_;
  FreezingVoterId aggregator_voter_id_;
};

TEST_F(FreezingVoteAggregatorTest, EndToEnd) {
  FreezingVotingChannel voter0 = aggregator()->GetVotingChannel();
  FreezingVotingChannel voter1 = aggregator()->GetVotingChannel();
  FreezingVotingChannel voter2 = aggregator()->GetVotingChannel();

  // Create some dummy votes for each PageNode and immediately expect
  // them to propagate upwards.
  voter0.SubmitVote(kPageNode0, kCannotFreezeVote0);
  voter0.SubmitVote(kPageNode1, kCanFreezeVote0);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCannotFreeze  =>  kCannotFreeze
  //    - kPageNode1: 1 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(2u, observer().GetVoteCount());
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kPageNode0,
                                 kCannotFreezeVote0));
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode1, kCanFreezeVote0));

  // Change an existing vote, and expect it to propagate upwards.
  voter0.ChangeVote(kPageNode0, kCanFreezeVote1);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 1 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(2u, observer().GetVoteCount());
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote1));
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode1, kCanFreezeVote0));

  // Submit a new kCanFreeze vote for page 1 and expect no change.
  voter1.SubmitVote(kPageNode1, kCanFreezeVote2);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 2 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(2u, observer().GetVoteCount());
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote1));
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode1, kCanFreezeVote0));

  // Submit a new vote with a different value and expect it to propagate
  // upwards.
  voter2.SubmitVote(kPageNode1, kCannotFreezeVote0);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 2 x kCanFreeze + 1 x kCannotFreeze    =>  kCannotFreeze
  EXPECT_EQ(2u, observer().GetVoteCount());
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote1));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kPageNode1,
                                 kCannotFreezeVote0));

  // Invalidate the only kCannotFreeze vote for a given PageNode and expect it
  // to propagate upwards.
  voter2.InvalidateVote(kPageNode1);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 2 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(2u, observer().GetVoteCount());
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote1));
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode1, kCanFreezeVote0));

  // Invalidate the remaining vote for kPageNode1.
  voter0.InvalidateVote(kPageNode1);
  voter1.InvalidateVote(kPageNode1);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: No vote            =>  Invalidated
  EXPECT_EQ(1u, observer().GetVoteCount());
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote1));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kPageNode1));

  // Emit a new vote for the PageNode that had no remaining vote
  voter0.SubmitVote(kPageNode1, kCannotFreezeVote1);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 1 x kCanFreeze     =>  kCannotFreeze
  EXPECT_EQ(2u, observer().GetVoteCount());
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote1));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kPageNode1,
                                 kCannotFreezeVote1));

  // Clear the votes.
  voter0.InvalidateVote(kPageNode0);
  voter0.InvalidateVote(kPageNode1);
}

TEST_F(FreezingVoteAggregatorTest, VoteIntegrity) {
  FreezingVotingChannel voter0 = aggregator()->GetVotingChannel();
  FreezingVotingChannel voter1 = aggregator()->GetVotingChannel();

  // Submit a first vote, this should be the only vote tracked by the
  // aggregator.
  voter0.SubmitVote(kPageNode0, kCanFreezeVote0);
  EXPECT_EQ(1u, FreezingVoteAggregatorTestAccess::GetAllVotes(aggregator(),
                                                              kPageNode0)
                    .size());
  EXPECT_EQ(kCanFreezeVote0, FreezingVoteAggregatorTestAccess::GetAllVotes(
                                 aggregator(), kPageNode0)
                                 .front()
                                 .second);
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote0));

  // Emit a second vote that should be ordered before the previous one, ensure
  // that this is the case.
  voter1.SubmitVote(kPageNode0, kCannotFreezeVote0);
  EXPECT_EQ(2u, FreezingVoteAggregatorTestAccess::GetAllVotes(aggregator(),
                                                              kPageNode0)
                    .size());
  EXPECT_EQ(kCannotFreezeVote0, FreezingVoteAggregatorTestAccess::GetAllVotes(
                                    aggregator(), kPageNode0)
                                    .front()
                                    .second);
  EXPECT_EQ(kCanFreezeVote0, FreezingVoteAggregatorTestAccess::GetAllVotes(
                                 aggregator(), kPageNode0)
                                 .back()
                                 .second);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kPageNode0,
                                 kCannotFreezeVote0));

  // Removing the second vote should restore things back to the state they were
  // before casting it.
  voter1.InvalidateVote(kPageNode0);
  EXPECT_EQ(1u, FreezingVoteAggregatorTestAccess::GetAllVotes(aggregator(),
                                                              kPageNode0)
                    .size());
  EXPECT_EQ(kCanFreezeVote0, FreezingVoteAggregatorTestAccess::GetAllVotes(
                                 aggregator(), kPageNode0)
                                 .front()
                                 .second);

  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote0));

  // Removing the last vote should cause the upstreamed vote to be invalidated.
  voter0.InvalidateVote(kPageNode0);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kPageNode0));
}

// Tests that submitting a second vote with the same value as the first one does
// not change the upstreamed vote.
TEST_F(FreezingVoteAggregatorTest, VoteConsistency) {
  FreezingVotingChannel voter0 = aggregator()->GetVotingChannel();
  FreezingVotingChannel voter1 = aggregator()->GetVotingChannel();

  // Submit a first vote.
  voter0.SubmitVote(kPageNode0, kCanFreezeVote0);
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote0));

  // Emit a second vote with the same value but a different reason so that they
  // can be differentiated. The upstreamed vote should be the same.
  voter1.SubmitVote(kPageNode0, kCanFreezeVote1);
  EXPECT_TRUE(
      observer().HasVote(aggregator_voter_id(), kPageNode0, kCanFreezeVote0));

  // Clear the votes.
  voter0.InvalidateVote(kPageNode0);
  voter1.InvalidateVote(kPageNode0);

  // Do the same with kCannotFreeze votes.

  // Submit a first vote.
  voter0.SubmitVote(kPageNode0, kCannotFreezeVote0);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kPageNode0,
                                 kCannotFreezeVote0));

  // Emit a second vote with the same value but a different reason so that they
  // can be differentiated. The upstreamed vote should be the same.
  voter1.SubmitVote(kPageNode0, kCannotFreezeVote1);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kPageNode0,
                                 kCannotFreezeVote0));

  // Clear the votes.
  voter0.InvalidateVote(kPageNode0);
  voter1.InvalidateVote(kPageNode0);
}

}  // namespace freezing
}  // namespace performance_manager
