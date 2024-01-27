// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_

#include <map>
#include <optional>

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
  void SetUpstreamVotingChannel(VotingChannel channel);

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
  class VoteData {
   public:
    enum class VoterType {
      kDefault,
      kOverride,
    };

    VoteData();
    VoteData(const VoteData& rhs) = delete;
    VoteData(VoteData&& rhs);
    VoteData& operator=(const VoteData& rhs) = delete;
    VoteData& operator=(VoteData&& rhs) = default;
    ~VoteData();

    void AddVote(VoterType voter_type, const Vote& vote);
    void ChangeVote(VoterType voter_type, const Vote& new_vote);
    void RemoveVote(VoterType voter_type);

    bool HasChosenVote() const;

    const Vote& GetChosenVote() const;

   private:
    // At least one of these is not null if a vote has been emitted for this
    // execution context.
    std::optional<Vote> default_vote_;
    std::optional<Vote> override_vote_;
  };

  using VoteDataMap = std::map<const ExecutionContext*, VoteData>;

  // Looks up the VoteData associated with the provided |vote|. The data is
  // expected to already exist (enforced by a DCHECK).
  VoteDataMap::iterator GetVoteData(const ExecutionContext* execution_context);

  // Returns the VoterType associated with |voter_id|.
  VoteData::VoterType GetVoterType(VoterId voter_id) const;

  // Our two input voters. We'll only accept votes from these voters otherwise
  // we'll DCHECK.
  VoterId override_voter_id_;
  VoterId default_voter_id_;

  // Our channel for upstreaming our votes.
  VotingChannel channel_;

  // Provides VotingChannels to our input voters.
  VotingChannelFactory voting_channel_factory_{this};

  // The votes we've upstreamed to our consumer.
  VoteDataMap vote_data_map_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_OVERRIDE_VOTE_AGGREGATOR_H_
