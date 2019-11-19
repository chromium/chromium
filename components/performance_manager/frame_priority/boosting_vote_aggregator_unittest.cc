// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/frame_priority/boosting_vote_aggregator.h"

#include "components/performance_manager/test_support/frame_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace frame_priority {

namespace {

// Some dummy frames.
const FrameNode* kFrame0 = reinterpret_cast<const FrameNode*>(0xF5A33000);
const FrameNode* kFrame1 = reinterpret_cast<const FrameNode*>(0xF5A33001);
const FrameNode* kFrame2 = reinterpret_cast<const FrameNode*>(0xF5A33002);
const FrameNode* kFrame3 = reinterpret_cast<const FrameNode*>(0xF5A33003);
const FrameNode* kFrame4 = reinterpret_cast<const FrameNode*>(0xF5A33004);

static constexpr base::TaskPriority kPriority0 = base::TaskPriority::LOWEST;
static constexpr base::TaskPriority kPriority1 =
    base::TaskPriority::USER_VISIBLE;
static constexpr base::TaskPriority kPriority2 = base::TaskPriority::HIGHEST;

static_assert(kPriority0 < kPriority1 && kPriority1 < kPriority2,
              "priorities must be well ordered");

static const char kReason0[] = "a reason";
static const char kReason1[] = "another reason";
static const char kReason2[] = "yet another reason";
static const char kReasonBoost[] = "boosted!";

class TestBoostingVoteAggregator : public BoostingVoteAggregator {
 public:
  using BoostingVoteAggregator::forward_edges_;
  using BoostingVoteAggregator::NodeData;
  using BoostingVoteAggregator::nodes_;
  using BoostingVoteAggregator::reverse_edges_;
};

using NodeData = TestBoostingVoteAggregator::NodeData;

class BoostingVoteAggregatorTest : public testing::Test {
 public:
  void SetUp() override {
    // Set up the chain such that |voter_| provides votes to |agg_|, which
    // upstreams them to |consumer_|.
    auto channel = consumer_.voting_channel_factory_.BuildVotingChannel();
    voter_id_ = channel.voter_id();
    agg_.SetUpstreamVotingChannel(std::move(channel));
    voter_.SetVotingChannel(agg_.GetVotingChannel());
    EXPECT_TRUE(agg_.nodes_.empty());
    EXPECT_TRUE(agg_.forward_edges_.empty());
    EXPECT_TRUE(agg_.reverse_edges_.empty());
  }

  void ExpectEdges(size_t count) {
    EXPECT_EQ(count, agg_.forward_edges_.size());
    EXPECT_EQ(count, agg_.reverse_edges_.size());
  }

  void ExpectIsActive(const NodeData& node_data,
                      bool mid_priority,
                      bool high_priority) {
    EXPECT_EQ(mid_priority, node_data.IsActive(1));
    EXPECT_EQ(high_priority, node_data.IsActive(2));
  }

  // The id of |agg_| as seen by its upstream |consumer_|.
  VoterId voter_id_ = 0;
  test::DummyVoteConsumer consumer_;
  TestBoostingVoteAggregator agg_;
  test::DummyVoter voter_;
};

}  // namespace

TEST_F(BoostingVoteAggregatorTest, VotesUpstreamingWorks) {
  // Submit a default vote to the boosting aggregator, and expect it not to be
  // upstreamed.
  voter_.EmitVote(kFrame0, kPriority0, kReason0);
  EXPECT_EQ(1u, agg_.nodes_.size());
  EXPECT_TRUE(voter_.receipts_[0].HasVote());
  ExpectEdges(0);
  EXPECT_TRUE(consumer_.votes_.empty());

  // Submit a non-default vote to the boosting aggregator, and expect it to be
  // upstreamed.
  voter_.EmitVote(kFrame1, kPriority1, kReason1);
  EXPECT_EQ(2u, agg_.nodes_.size());
  EXPECT_TRUE(voter_.receipts_[0].HasVote());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());
  ExpectEdges(0);
  EXPECT_EQ(1u, consumer_.votes_.size());
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReason1);

  // Make vote 0 non-default and expect it to be upstreamed.
  voter_.receipts_[0].ChangeVote(kPriority2, kReason2);
  EXPECT_EQ(2u, agg_.nodes_.size());
  EXPECT_TRUE(voter_.receipts_[0].HasVote());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());
  ExpectEdges(0);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReason1);
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority2, kReason2);

  // Make vote 1 default and expect the upstream vote to be canceled.
  voter_.receipts_[1].ChangeVote(kPriority0, kReason0);
  EXPECT_EQ(2u, agg_.nodes_.size());
  EXPECT_TRUE(voter_.receipts_[0].HasVote());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());
  ExpectEdges(0);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectInvalidVote(0);
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority2, kReason2);

  // Change the reason but not the priority of vote 0 and expect the upstream
  // vote to change as well.
  voter_.receipts_[0].ChangeVote(kPriority2, kReason0);
  EXPECT_EQ(2u, agg_.nodes_.size());
  EXPECT_TRUE(voter_.receipts_[0].HasVote());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());
  ExpectEdges(0);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectInvalidVote(0);
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority2, kReason0);

  // Cancel vote 0 and expect it to be canceled upstream.
  voter_.receipts_[0].Reset();
  EXPECT_EQ(1u, agg_.nodes_.size());
  EXPECT_FALSE(voter_.receipts_[0].HasVote());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());
  ExpectEdges(0);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectInvalidVote(0);
  consumer_.ExpectInvalidVote(1);

  // Cancel vote 1 and expect no change to the upstream votes.
  voter_.receipts_[1].Reset();
  EXPECT_EQ(0u, agg_.nodes_.size());
  EXPECT_FALSE(voter_.receipts_[0].HasVote());
  EXPECT_FALSE(voter_.receipts_[1].HasVote());
  ExpectEdges(0);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectInvalidVote(0);
  consumer_.ExpectInvalidVote(1);
}

TEST_F(BoostingVoteAggregatorTest, BoostingWorks) {
  // Add a boosting vote, with no actual incoming votes. This should produce
  // the two nodes associated with the edge but not upstream any votes.
  BoostingVote boost01a(&agg_, kFrame0, kFrame1, kReasonBoost);
  const auto& data0 = agg_.nodes_.find(kFrame0)->second;
  const auto& data1 = agg_.nodes_.find(kFrame1)->second;
  EXPECT_TRUE(voter_.receipts_.empty());
  EXPECT_EQ(2u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_TRUE(consumer_.votes_.empty());
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);

  // Create a second boosting vote. This duplicates the edge.
  BoostingVote boost01b(&agg_, kFrame0, kFrame1, kReasonBoost);
  EXPECT_TRUE(voter_.receipts_.empty());
  EXPECT_EQ(2u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_TRUE(consumer_.votes_.empty());
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);

  // Create a mid priority vote for frame 1. This should cause a single vote
  // to be emitted for that frame.
  voter_.EmitVote(kFrame1, kPriority1, kReason1);
  EXPECT_EQ(1u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[0].HasVote());  // kFrame1.
  EXPECT_EQ(2u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(1u, consumer_.votes_.size());
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReason1);
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, true, false);

  // Create a mid priority vote for frame 0. This should cause another vote to
  // be emitted.
  voter_.EmitVote(kFrame0, kPriority1, kReason1);
  EXPECT_EQ(2u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());  // kFrame0.
  EXPECT_TRUE(voter_.receipts_[0].HasVote());  // kFrame1.
  EXPECT_EQ(2u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReason1);
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);

  // Cancel the priority 1 vote for frame 1. The boosting should maintain the
  // output priority for that node.
  voter_.receipts_[0].Reset();  // kFrame1.
  EXPECT_EQ(2u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_EQ(2u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReasonBoost);
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);

  // Create a default vote for a third frame. Other than creating the node data
  // and the vote this shouldn't do anything.
  voter_.EmitVote(kFrame2, kPriority0, kReason0);
  const auto& data2 = agg_.nodes_.find(kFrame2)->second;
  EXPECT_EQ(3u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_TRUE(voter_.receipts_[2].HasVote());   // kFrame2.
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReasonBoost);
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(0u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Create a boosting vote from frame 2 to frame 0. This should create an edge.
  BoostingVote boost20(&agg_, kFrame2, kFrame0, kReasonBoost);
  EXPECT_EQ(3u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_TRUE(voter_.receipts_[2].HasVote());   // kFrame2.
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(2u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReasonBoost);
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Emit a highest priority vote for frame 2. This should boost frames 0 and
  // 1 as well.
  voter_.receipts_[2].ChangeVote(kPriority2, kReason2);  // kFrame2.
  EXPECT_EQ(3u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_TRUE(voter_.receipts_[2].HasVote());   // kFrame2.
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority2, kReasonBoost);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority2, kReasonBoost);
  consumer_.ExpectValidVote(2, voter_id_, kFrame2, kPriority2, kReason2);
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, true);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, false, true);

  // Emit a highest priority vote for frame 1. This should change the vote
  // reason.
  voter_.EmitVote(kFrame1, kPriority2, kReason2);
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_TRUE(voter_.receipts_[3].HasVote());   // kFrame1.
  EXPECT_TRUE(voter_.receipts_[2].HasVote());   // kFrame2.
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority2,
                            kReasonBoost);  // kFrame0.
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority2,
                            kReason2);  // kFrame1.
  consumer_.ExpectValidVote(2, voter_id_, kFrame2, kPriority2,
                            kReason2);  // kFrame2.
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, true);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, false, true);

  // Kill the vote for frame 2. This should kill the upstream vote for frame 2
  // entirely, reduce the priority of frame 0, and keep frame 1 the same.
  voter_.receipts_[2].Reset();  // kFrame2.
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_TRUE(voter_.receipts_[3].HasVote());   // kFrame1.
  EXPECT_FALSE(voter_.receipts_[2].HasVote());  // Old kFrame2.
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority2, kReason2);
  consumer_.ExpectInvalidVote(2);  // Old kFrame2.
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, false, false);

  // Kill the direct vote for frame 1 so it goes back to being boosted by
  // frame 0.
  voter_.receipts_[3].Reset();
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[3].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[2].HasVote());  // Old kFrame2.
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReasonBoost);
  consumer_.ExpectInvalidVote(2);  // Old kFrame2.
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Kill the first boosting vote from 0 to 1. This should do nothing but change
  // edge the multiplicity of the edge.
  boost01a.Reset();
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[3].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[2].HasVote());  // Old kFrame2.
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectValidVote(0, voter_id_, kFrame1, kPriority1, kReasonBoost);
  consumer_.ExpectInvalidVote(2);  // Old kFrame2.
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Kill the second boosting vote from 0 to 1. This should change edge counts,
  // and remove both the vote and the node data. The variable |data1| is now
  // invalid.
  boost01b.Reset();
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[3].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[2].HasVote());  // Old kFrame2.
  EXPECT_EQ(2u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectInvalidVote(0);  // Old kFrame1.
  consumer_.ExpectInvalidVote(2);  // Old kFrame2.
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data2, false, false);

  // Move the boosting vote. The move should not cause any outwardly visible
  // changes.
  BoostingVote boost20b(std::move(boost20));
  EXPECT_EQ(&agg_, boost20b.aggregator());
  EXPECT_EQ(kFrame2, boost20b.input_frame());
  EXPECT_EQ(kFrame0, boost20b.output_frame());
  EXPECT_EQ(kReasonBoost, boost20b.reason());
  EXPECT_FALSE(boost20.aggregator());
  EXPECT_FALSE(boost20.input_frame());
  EXPECT_FALSE(boost20.output_frame());
  EXPECT_FALSE(boost20.reason());
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[3].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[2].HasVote());  // Old kFrame2.
  EXPECT_EQ(2u, agg_.nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectInvalidVote(0);  // Old kFrame1.
  consumer_.ExpectInvalidVote(2);  // Old kFrame2.
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data2, false, false);

  // Remove the boosting vote from 2 to 0. This should change edge counts, and
  // also remove the node data associated with node 2. |data2| is now invalid.
  boost20b.Reset();
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_TRUE(voter_.receipts_[1].HasVote());   // kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[3].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[2].HasVote());  // Old kFrame2.
  EXPECT_EQ(1u, agg_.nodes_.size());
  ExpectEdges(0);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectValidVote(1, voter_id_, kFrame0, kPriority1, kReason1);
  consumer_.ExpectInvalidVote(0);  // Old kFrame1.
  consumer_.ExpectInvalidVote(2);  // Old kFrame2.
  EXPECT_EQ(0u, data0.edge_count_for_testing());
  ExpectIsActive(data0, true, false);

  // Finally remove the last vote. The aggregator should effectively be empty at
  // this point. |data0| also becomes invalid after this.
  voter_.receipts_[1].Reset();
  EXPECT_EQ(4u, voter_.receipts_.size());
  EXPECT_FALSE(voter_.receipts_[1].HasVote());  // Old kFrame0.
  EXPECT_FALSE(voter_.receipts_[0].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[3].HasVote());  // Old kFrame1.
  EXPECT_FALSE(voter_.receipts_[2].HasVote());  // Old kFrame2.
  EXPECT_EQ(0u, agg_.nodes_.size());
  ExpectEdges(0);
  EXPECT_EQ(3u, consumer_.votes_.size());
  consumer_.ExpectInvalidVote(1);  // Old kFrame0.
  consumer_.ExpectInvalidVote(0);  // Old kFrame1.
  consumer_.ExpectInvalidVote(2);  // Old kFrame2.
}

TEST_F(BoostingVoteAggregatorTest, DiamondPattern) {
  // Create a diamond boosting vote pattern:
  //
  //   1
  //  / \
  // 0   3
  //  \ /
  //   2
  BoostingVote boost01(&agg_, kFrame0, kFrame1, kReasonBoost);
  BoostingVote boost02(&agg_, kFrame0, kFrame2, kReasonBoost);
  BoostingVote boost13(&agg_, kFrame1, kFrame3, kReasonBoost);
  BoostingVote boost23(&agg_, kFrame2, kFrame3, kReasonBoost);

  const auto& data0 = agg_.nodes_.find(kFrame0)->second;
  const auto& data1 = agg_.nodes_.find(kFrame1)->second;
  const auto& data2 = agg_.nodes_.find(kFrame2)->second;
  const auto& data3 = agg_.nodes_.find(kFrame3)->second;
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);

  // Add a vote to node 0. This should cause all nodes to be boosted.
  voter_.EmitVote(kFrame0, kPriority2, kReason2);
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);
  ExpectIsActive(data3, false, true);

  // Cancel the vote. All boosting should disappear.
  voter_.receipts_.clear();
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);
}

TEST_F(BoostingVoteAggregatorTest, DiamondPatternMultipleVotes) {
  // Create another diamond boosting vote pattern:
  //
  //       1
  //      / \
  // 4 - 0   3
  //      \ /
  //       2
  BoostingVote boost01(&agg_, kFrame0, kFrame1, kReasonBoost);
  BoostingVote boost02(&agg_, kFrame0, kFrame2, kReasonBoost);
  BoostingVote boost13(&agg_, kFrame1, kFrame3, kReasonBoost);
  BoostingVote boost23(&agg_, kFrame2, kFrame3, kReasonBoost);

  const auto& data0 = agg_.nodes_.find(kFrame0)->second;
  const auto& data1 = agg_.nodes_.find(kFrame1)->second;
  const auto& data2 = agg_.nodes_.find(kFrame2)->second;
  const auto& data3 = agg_.nodes_.find(kFrame3)->second;
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);

  // Add a vote to node 0. This should cause all downstream nodes to be boosted.
  voter_.EmitVote(kFrame0, kPriority2, kReason2);
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);
  ExpectIsActive(data3, false, true);

  // Add a lower vote to frame0 via frame4. This should also propagate through
  // the network in a similar way.
  BoostingVote boost40(&agg_, kFrame4, kFrame0, kReasonBoost);
  const auto& data4 = agg_.nodes_.find(kFrame4)->second;
  voter_.EmitVote(kFrame4, kPriority1, kReason1);
  ExpectIsActive(data0, true, true);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, true, true);
  ExpectIsActive(data3, true, true);
  ExpectIsActive(data4, true, false);
}

TEST_F(BoostingVoteAggregatorTest, RemoveEdgeFromCycle) {
  BoostingVote boost01(&agg_, kFrame0, kFrame1, kReasonBoost);
  BoostingVote boost12(&agg_, kFrame1, kFrame2, kReasonBoost);
  BoostingVote boost23(&agg_, kFrame2, kFrame3, kReasonBoost);
  BoostingVote boost30(&agg_, kFrame3, kFrame0, kReasonBoost);

  const auto& data0 = agg_.nodes_.find(kFrame0)->second;
  const auto& data1 = agg_.nodes_.find(kFrame1)->second;
  const auto& data2 = agg_.nodes_.find(kFrame2)->second;
  const auto& data3 = agg_.nodes_.find(kFrame3)->second;
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);

  // Add a vote to node 0.
  voter_.EmitVote(kFrame0, kPriority2, kReason2);
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);
  ExpectIsActive(data3, false, true);

  // Remove an edge from the cycle. The first half of the cycle should still
  // be boosted, the second half should not.
  boost12.Reset();
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);
}

TEST_F(BoostingVoteAggregatorTest, MoveCancelsPreviousBoostingVote) {
  BoostingVote boost01(&agg_, kFrame0, kFrame1, kReasonBoost);
  BoostingVote boost12(&agg_, kFrame1, kFrame2, kReasonBoost);

  // Expect nodes to have been created for all nodes involved in boosting votes.
  EXPECT_TRUE(agg_.nodes_.count(kFrame0));
  EXPECT_TRUE(agg_.nodes_.count(kFrame1));
  EXPECT_TRUE(agg_.nodes_.count(kFrame2));

  // Move one boosting vote into the other. This should cause the latter to be
  // canceled. In this case that means node0 should be removed.
  boost01 = std::move(boost12);
  EXPECT_FALSE(agg_.nodes_.count(kFrame0));
  EXPECT_TRUE(agg_.nodes_.count(kFrame1));
  EXPECT_TRUE(agg_.nodes_.count(kFrame2));
}

TEST_F(BoostingVoteAggregatorTest, BoostingVoteAfterNormalVotes) {
  voter_.EmitVote(kFrame0, kPriority2, kReason2);
  EXPECT_TRUE(agg_.nodes_.count(kFrame0));
  EXPECT_EQ(1u, agg_.nodes_.size());
  const auto& data0 = agg_.nodes_.find(kFrame0)->second;
  ExpectIsActive(data0, false, true);

  BoostingVote boost12(&agg_, kFrame1, kFrame2, kReasonBoost);
  EXPECT_TRUE(agg_.nodes_.count(kFrame0));
  EXPECT_TRUE(agg_.nodes_.count(kFrame1));
  EXPECT_TRUE(agg_.nodes_.count(kFrame2));
  EXPECT_EQ(3u, agg_.nodes_.size());
  const auto& data1 = agg_.nodes_.find(kFrame1)->second;
  const auto& data2 = agg_.nodes_.find(kFrame2)->second;
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);

  BoostingVote boost01(&agg_, kFrame0, kFrame1, kReasonBoost);
  EXPECT_TRUE(agg_.nodes_.count(kFrame0));
  EXPECT_TRUE(agg_.nodes_.count(kFrame1));
  EXPECT_TRUE(agg_.nodes_.count(kFrame2));
  EXPECT_EQ(3u, agg_.nodes_.size());
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);
}

}  // namespace frame_priority
}  // namespace performance_manager
