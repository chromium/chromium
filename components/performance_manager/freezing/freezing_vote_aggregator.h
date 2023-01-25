// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_VOTE_AGGREGATOR_H_

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/values.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/voting/voting.h"

namespace performance_manager {

class FreezingVoteDecorator;
class PageNode;

namespace freezing {

// An aggregator for freezing votes. It upstreams an aggregated vote to an
// upstream channel every time the freezing decision changes for a PageNode. It
// allows freezing of a given PageNode upon reception of one or several
// kCanFreeze vote for this node. Any kCannotFreeze vote received will have
// priority over the kCanFreeze votes and will prevent the PageNode from being
// frozen.
//
// This is a GraphRegistered object, once created this instance can be
// retrieved via:
//     graph()->GetRegisteredObjectAs<freezing::FreezingVoteAggregator>();
class FreezingVoteAggregator final
    : public FreezingVoteObserver,
      public NodeDataDescriberDefaultImpl,
      public GraphRegisteredImpl<FreezingVoteAggregator> {
 public:
  FreezingVoteAggregator(const FreezingVoteAggregator& rhs) = delete;
  FreezingVoteAggregator& operator=(const FreezingVoteAggregator& rhs) = delete;
  ~FreezingVoteAggregator() override;

  // Issues a voting channel (effectively registered a voter).
  FreezingVotingChannel GetVotingChannel();

  // Sets the upstream voting channel. Should only be called once.
  void SetUpstreamVotingChannel(FreezingVotingChannel&& channel);

  // FreezingVoteObserver implementation:
  void OnVoteSubmitted(FreezingVoterId voter_id,
                       const PageNode* page_node,
                       const FreezingVote& vote) override;
  void OnVoteChanged(FreezingVoterId voter_id,
                     const PageNode* page_node,
                     const FreezingVote& new_vote) override;
  void OnVoteInvalidated(FreezingVoterId voter_id,
                         const PageNode* page_node) override;

  void RegisterNodeDataDescriber(Graph* graph);
  void UnregisterNodeDataDescriber(Graph* graph);

  // NodeDataDescriber implementation:
  base::Value DescribePageNodeData(const PageNode* node) const override;

 private:
  friend class performance_manager::FreezingVoteDecorator;
  friend class FreezingVoteAggregatorTest;
  friend class FreezingVoteAggregatorTestAccess;

  // Private constructor, in practice the FreezingVoteDecorator is responsible
  // for maintaining the lifetime of this object.
  FreezingVoteAggregator();

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

    // Adds a new vote.
    void AddVote(FreezingVoterId voter_id, const FreezingVote& vote);

    // Updates an existing vote.
    void UpdateVote(FreezingVoterId voter_id, const FreezingVote& new_vote);

    // Removes an existing vote
    void RemoveVote(FreezingVoterId voter_id);

    // Upstreams the vote for this vote data, using the given voting |channel|.
    void UpstreamVote(const PageNode* page_node,
                      FreezingVotingChannel* channel);

    bool IsEmpty() { return votes_.empty(); }

    // Returns the chosen vote. Invalid to call if IsEmpty() is true.
    const FreezingVote& GetChosenVote();

    // Helper for FreezingVoteAggregator::DescribePageNodeData.
    void DescribeVotes(base::Value::Dict& ret) const;

   private:
    friend class FreezingVoteAggregatorTestAccess;

    // The current set of votes.
    using VotesDeque =
        base::circular_deque<std::pair<FreezingVoterId, FreezingVote>>;

    const VotesDeque& GetVotesForTesting() { return votes_; }

    // Returns the iterator of |voter_id| in |votes_|. |voter_id| is expected
    // to be in the deque, this is enforced by a DCHECK.
    VotesDeque::iterator FindVote(FreezingVoterId voter_id);

    void AddVoteToDeque(FreezingVoterId voter_id, const FreezingVote& vote);

    // kCannotFreeze votes are always at the beginning of the deque.
    VotesDeque votes_;
  };
  using VoteDataMap = base::flat_map<const PageNode*, FreezingVoteData>;

  // Looks up the VoteData associated with the provided |page_node|. The data is
  // expected to already exist (enforced by a DCHECK).
  VoteDataMap::iterator GetVoteData(const PageNode* page_node);

  // A map that associates a PageNode with a FreezingVoteData structure.
  VoteDataMap vote_data_map_;

  // The channel for upstreaming our votes.
  FreezingVotingChannel channel_;

  // Provides FreezingVotingChannels to our input voters.
  FreezingVotingChannelFactory freezing_voting_channel_factory_{this};
};

}  // namespace freezing
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_VOTE_AGGREGATOR_H_
