// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_

#include <map>

#include "components/performance_manager/public/frame_priority/frame_priority.h"

namespace performance_manager {
namespace frame_priority {

// Aggregator that allows votes from 2 different Voters, where one of the voters
// is allowed to override the votes of another. This aggregator should be
// completely setup before any votes are submitted to it.
class OverrideVoteAggregator : public VoteConsumer {
 public:
  OverrideVoteAggregator();
  ~OverrideVoteAggregator() override;

  // All 3 of these must have been called in order for the aggregator to be
  // fully setup.
  VotingChannel GetOverrideVotingChannel();
  VotingChannel GetDefaultVotingChannel();
  void SetUpstreamVotingChannel(VotingChannel&& channel);

  bool IsSetup() const;

  size_t GetSizeForTesting() const { return vote_data_map_.size(); }

 protected:
  // VoteConsumer implementation:
  VoteReceipt SubmitVote(VoterId voter_id, const Vote& vote) override;
  VoteReceipt ChangeVote(VoteReceipt receipt,
                         AcceptedVote* vote,
                         const Vote& old_vote) override;
  void VoteInvalidated(AcceptedVote* vote) override;

 private:
  // This is move-only because all of its members are move-only.
  struct VoteData {
    VoteData() = default;
    VoteData(const VoteData& rhs) = delete;
    VoteData(VoteData&& rhs) = default;
    VoteData& operator=(const VoteData& rhs) = delete;
    VoteData& operator=(VoteData&& rhs) = default;
    ~VoteData() = default;

    // Each of these IsValid if a vote has been emitted for this frame,
    // otherwise !IsValid. At least one of the votes must be valid, otherwise
    // the entire map entry will be destroyed.
    AcceptedVote override_vote;
    AcceptedVote default_vote;

    // The receipt for the vote we've upstreamed.
    VoteReceipt receipt;
  };

  using VoteDataMap = std::map<const FrameNode*, VoteData>;

  // Looks up the VoteData associated with the provided |vote|. The data is
  // expected to already exist (enforced by a DCHECK).
  VoteDataMap::iterator GetVoteData(AcceptedVote* vote);

  // Rebrands |vote| as belonging to this voter, and then sends it along to our
  // |consumer_|. Stores the resulting receipt in |vote_data|.
  void UpstreamVote(const Vote& vote, VoteData* vote_data);

  // Our two input voters. We'll only accept votes from these voters otherwise
  // we'll DCHECK.
  VoterId override_voter_id_ = kInvalidVoterId;
  VoterId default_voter_id_ = kInvalidVoterId;

  // Our channel for upstreaming our votes.
  VotingChannel channel_;

  // Our VotingChannelFactory for providing VotingChannels to our input voters.
  VotingChannelFactory factory_;

  // The votes we've upstreamed to our consumer.
  VoteDataMap vote_data_map_;

  DISALLOW_COPY_AND_ASSIGN(OverrideVoteAggregator);
};

}  // namespace frame_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_
