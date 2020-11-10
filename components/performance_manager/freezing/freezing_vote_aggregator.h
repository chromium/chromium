// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_VOTE_AGGREGATOR_H_

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "components/performance_manager/public/voting/voting.h"

namespace performance_manager {

class PageNode;

namespace freezing {

enum class FreezingVoteValue {
  kCannotFreeze,
  kCanFreeze,
};

using FreezingVote =
    voting::Vote<PageNode, FreezingVoteValue, FreezingVoteValue::kCannotFreeze>;
using FreezingVoteReceipt = voting::VoteReceipt<FreezingVote>;
using FreezingVotingChannel = voting::VotingChannel<FreezingVote>;
using FreezingVoteConsumer = voting::VoteConsumer<FreezingVote>;
using AcceptedFreezingVote = voting::AcceptedVote<FreezingVote>;
using FreezingVotingChannelFactory = voting::VotingChannelFactory<FreezingVote>;

// An aggregator for freezing votes. It upstreams an aggregated vote to an
// upstream channel every time the freezing decision changes for a PageNode. It
// allows freezing of a given PageNode upon reception of one or several
// kCanFreeze vote for this node. Any kCannotFreeze vote received will have
// priority over the kCanFreeze votes and will prevent the PageNode from being
// frozen.
class FreezingVoteAggregator final : public FreezingVoteConsumer {
 public:
  FreezingVoteAggregator();
  FreezingVoteAggregator(const FreezingVoteAggregator& rhs) = delete;
  FreezingVoteAggregator& operator=(const FreezingVoteAggregator& rhs) = delete;
  ~FreezingVoteAggregator() override;

  // Issues a voting channel (effectively registered a voter).
  FreezingVotingChannel GetVotingChannel();

  // Sets the upstream voting channel. Should only be called once.
  void SetUpstreamVotingChannel(FreezingVotingChannel&& channel);

  // VoteConsumer implementation:
  FreezingVoteReceipt SubmitVote(util::PassKey<FreezingVotingChannel>,
                                 voting::VoterId<FreezingVote> voter_id,
                                 const PageNode* page_node,
                                 const FreezingVote& vote) override;
  void ChangeVote(util::PassKey<AcceptedFreezingVote>,
                  AcceptedFreezingVote* old_vote,
                  const FreezingVote& new_vote) override;
  void VoteInvalidated(util::PassKey<AcceptedFreezingVote>,
                       AcceptedFreezingVote* vote) override;

 private:
  friend class FreezingVoteAggregatorTestAccess;

  // Contains the freezing votes for a given PageNode.
  class FreezingVoteData {
   public:
    // The consequence that adding, removing or updating a vote has on the
    // upstreamed vote. The caller is responsible for calling UpstreamVote or
    // invalidating the vote (by destroying the instance of this class that owns
    // it).
    enum class UpstreamVoteImpact {
      // The upstream vote has changed. UpstreamVote should be called.
      kUpstreamVoteChanged,
      // The upstream vote has been removed and should be invalidated.
      kUpstreamVoteRemoved,
      // The operation had no impact on the upstreamed vote.
      kUpstreamVoteUnchanged,
    };

    FreezingVoteData();
    FreezingVoteData(FreezingVoteData&&);
    FreezingVoteData& operator=(FreezingVoteData&&);
    FreezingVoteData(const FreezingVoteData& rhs) = delete;
    FreezingVoteData& operator=(const FreezingVoteData& rhs) = delete;
    ~FreezingVoteData();

    // Adds a vote. Returns an UpstreamVoteImpact indicating if the upstreamed
    // vote should be updated by calling UpstreamVote.
    UpstreamVoteImpact AddVote(AcceptedFreezingVote&& vote) WARN_UNUSED_RESULT;

    // Updates a vote. Returns an UpstreamVoteImpact indicating if the
    // upstreamed vote should be updated by calling UpstreamVote.
    UpstreamVoteImpact UpdateVote(AcceptedFreezingVote* old_vote,
                                  const FreezingVote& new_vote)
        WARN_UNUSED_RESULT;

    // Removes a vote. Returns an UpstreamVoteImpact indicating if the
    // upstreamed vote should be updated by calling UpstreamVote or invalidated.
    UpstreamVoteImpact RemoveVote(AcceptedFreezingVote* vote)
        WARN_UNUSED_RESULT;

    // Upstreams the vote for this vote data, using the given voting |channel|.
    void UpstreamVote(FreezingVotingChannel* channel);

    bool IsEmpty() { return accepted_votes_.empty(); }

    // Returns the current aggregated vote.
    const AcceptedFreezingVote& GetCurrentVote();

    friend class FreezingVoteAggregatorTestAccess;

    // The current set of votes.
    using AcceptedVotesDeque = base::circular_deque<AcceptedFreezingVote>;

    const AcceptedVotesDeque& GetAcceptedVotesForTesting() {
      return accepted_votes_;
    }

    // Returns the iterator of |vote| in |accepted_votes_|. |vote| is expected
    // to be in the deque, this is enforced by a DCHECK.
    AcceptedVotesDeque::iterator FindVote(AcceptedFreezingVote* vote);

    void AddVoteToDeque(AcceptedFreezingVote&& vote);

    // kCannotFreeze votes are always at the beginning of the deque.
    AcceptedVotesDeque accepted_votes_;

    // The receipt for the vote we've upstreamed.
    FreezingVoteReceipt receipt_;
  };
  using VoteDataMap = base::flat_map<const PageNode*, FreezingVoteData>;

  // Looks up the VoteData associated with the provided |page_node|. The data is
  // expected to already exist (enforced by a DCHECK).
  VoteDataMap::iterator GetVoteData(const PageNode* page_node);

  // A map that associates a PageNode with a FreezingVoteData structure.
  VoteDataMap vote_data_map_;

  // The channel for upstreaming our votes.
  FreezingVotingChannel channel_;

  // The factory for providing FreezingVotingChannels to our input voters.
  FreezingVotingChannelFactory factory_;
};

}  // namespace freezing
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_VOTE_AGGREGATOR_H_
