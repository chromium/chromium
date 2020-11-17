// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_

#include <map>

#include "base/optional.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

namespace performance_manager {
namespace execution_context_priority {

// Aggregator that allows votes from 2 different Voters, where one of the voters
// is allowed to override the votes of another. This aggregator should be
// completely setup before any votes are submitted to it.
class OverrideVoteAggregator : public VoteObserver {
 public:
  OverrideVoteAggregator();
  ~OverrideVoteAggregator() override;

  OverrideVoteAggregator(const OverrideVoteAggregator&) = delete;
  OverrideVoteAggregator& operator=(const OverrideVoteAggregator&) = delete;

  // All 3 of these must have been called in order for the aggregator to be
  // fully setup.
  VotingChannel GetOverrideVotingChannel();
  VotingChannel GetDefaultVotingChannel();
  void SetUpstreamVotingChannel(VotingChannel&& channel);

  bool IsSetup() const;

  size_t GetSizeForTesting() const { return vote_data_map_.size(); }

 protected:
  // VoteObserver implementation:
  void OnVoteSubmitted(VoterId voter_id,
                       const ExecutionContext* execution_context,
                       const Vote& vote) override;
  void OnVoteChanged(VoterId voter_id,
                     const ExecutionContext* execution_context,
                     const Vote& new_vote) override;
  void OnVoteInvalidated(VoterId voter_id,
                         const ExecutionContext* execution_context) override;

 private:
  // This is move-only because all of its members are move-only.
  struct VoteData {
    VoteData();
    VoteData(const VoteData& rhs) = delete;
    VoteData(VoteData&& rhs);
    VoteData& operator=(const VoteData& rhs) = delete;
    VoteData& operator=(VoteData&& rhs) = default;
    ~VoteData();

    // Each of these is not null if a vote has been emitted for this execution
    // context. At least one of the votes must exist, otherwise the entire map
    // entry will be destroyed.
    base::Optional<Vote> override_vote;
    base::Optional<Vote> default_vote;

    // The receipt for the vote we've upstreamed.
    VoteReceipt receipt;
  };

  using VoteDataMap = std::map<const ExecutionContext*, VoteData>;

  // Looks up the VoteData associated with the provided |vote|. The data is
  // expected to already exist (enforced by a DCHECK).
  VoteDataMap::iterator GetVoteData(const ExecutionContext* execution_context);

  // Rebrands |vote| as belonging to this voter, and then sends it along to our
  // |consumer_|. Stores the resulting receipt in |vote_data|.
  void UpstreamVote(const ExecutionContext* execution_context,
                    const Vote& vote,
                    VoteData* vote_data);

  // Our two input voters. We'll only accept votes from these voters otherwise
  // we'll DCHECK.
  VoterId override_voter_id_ = voting::kInvalidVoterId<Vote>;
  VoterId default_voter_id_ = voting::kInvalidVoterId<Vote>;

  // Our channel for upstreaming our votes.
  VotingChannel channel_;

  // Provides VotingChannels to our input voters.
  VoteConsumerDefaultImpl vote_consumer_default_impl_;

  // The votes we've upstreamed to our consumer.
  VoteDataMap vote_data_map_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_
