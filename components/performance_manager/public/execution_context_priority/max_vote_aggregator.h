// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_MAX_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_MAX_VOTE_AGGREGATOR_H_

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/intrusive_heap.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

namespace performance_manager {
namespace execution_context_priority {

// Aggregator that allows votes from an arbitrary number of voters, and notifies
// its observers of the maximum vote for each vote context. New voting channels
// may be issued at any time during its lifetime.
//
// Vote contexts are aggregated independently of each other. As a result,
// observers may re-entrantly cast votes into this aggregator on other vote
// contexts. A re-entrant change of the top vote of the vote context whose
// observers are being notified indicates a cycle, and is CHECKed.
class MaxVoteAggregator : public VoteObserver {
 public:
  // Notified when the top vote for a node changes. `vote` is nullopt when the
  // last vote for the node is removed.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnFrameTopVoteChanged(const FrameNode* frame_node,
                                       const std::optional<Vote>& vote) {}
    virtual void OnWorkerTopVoteChanged(const WorkerNode* worker_node,
                                        const std::optional<Vote>& vote) {}
  };

  MaxVoteAggregator();
  MaxVoteAggregator(const MaxVoteAggregator&) = delete;
  MaxVoteAggregator& operator=(const MaxVoteAggregator&) = delete;
  ~MaxVoteAggregator() override;

  // Issues a voting channel (effectively registered a voter).
  VotingChannel GetVotingChannel();

  // Observers are notified in the order they are added.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if at least one vote is currently cast on `vote_context`.
  bool HasVotes(VoteContext vote_context) const;

 protected:
  // VoteObserver implementation:
  void OnVoteSet(VoterId voter_id,
                 VoteContext vote_context,
                 const std::optional<Vote>& vote) override;

 private:
  friend class MaxVoteAggregatorTestAccess;

  // A StampedVote is a Vote with a serial number that can be used to order
  // votes by the order in which they were received. This ensures that votes
  // upstreamed by this aggregator remain as stable as possible.
  class StampedVote : public base::InternalHeapHandleStorage {
   public:
    StampedVote();
    StampedVote(const Vote& vote, uint32_t vote_id);
    StampedVote(StampedVote&&);
    StampedVote(const StampedVote&) = delete;
    ~StampedVote() override;

    StampedVote& operator=(StampedVote&&) = default;
    StampedVote& operator=(const StampedVote&) = delete;

    bool operator<(const StampedVote& rhs) const {
      if (vote_.value() != rhs.vote_.value()) {
        return vote_.value() < rhs.vote_.value();
      }
      // Higher |vote_id| values are of lower priority.
      return vote_id_ > rhs.vote_id_;
    }

    const Vote& vote() const { return vote_; }
    uint32_t vote_id() const { return vote_id_; }

    void SetVote(const Vote& new_vote) { vote_ = new_vote; }

   private:
    Vote vote_;
    uint32_t vote_id_ = 0;
  };

  // The collection of votes for a single vote context. This is move-only
  // because all of its members are move-only. Internally it houses the
  // collection of all votes associated with a vote context as max-heap,
  // and a map of HeapHandles to access existing votes.
  class VoteData {
   public:
    VoteData();
    VoteData(const VoteData& rhs) = delete;
    VoteData(VoteData&& rhs);
    VoteData& operator=(const VoteData& rhs) = delete;
    VoteData& operator=(VoteData&& rhs);
    ~VoteData();

    // Adds or updates a vote cast by |voter_id|. Returns true if the vote was
    // added or modified, or false if the vote was already present with the
    // exact same value.
    bool SetVote(VoterId voter_id, const Vote& vote, uint32_t vote_id);

    // Removes an existing vote cast by |voter_id|.
    void RemoveVote(VoterId voter_id);

    // Checks if a vote cast by |voter_id| exists.
    bool HasVote(VoterId voter_id) const;

    // Returns the top vote, or nullopt if empty.
    std::optional<Vote> GetTopVote() const;

   private:
    base::IntrusiveHeap<StampedVote> votes_;

    // Maps each voting channel to the HeapHandle to their associated vote in
    // |votes_|.
    std::map<VoterId, raw_ptr<base::HeapHandle, CtnExperimental>> heap_handles_;
  };

  using VoteDataMap = std::map<VoteContext, VoteData>;

  // Notifies the observers that the top vote for `vote_context` changed.
  void NotifyTopVoteChanged(VoteContext vote_context,
                            const std::optional<Vote>& vote);

  // Votes can be cast while observers are being notified (e.g. setting the
  // priority of a frame changes the vote on its child frames), so notifications
  // are re-entrant. Cycles are caught by `notifying_vote_contexts_` instead.
  base::ReentrantObserverList<Observer> observers_;

  // The vote contexts whose observers are being notified, innermost last.
  // Notifications are strictly nested, so this is a stack.
  std::vector<VoteContext> notifying_vote_contexts_;

  // Provides VotingChannels to our input voters.
  VotingChannelFactory voting_channel_factory_{this};

  // The next StampedVote ID to use.
  uint32_t next_vote_id_ = 0;

  // Received votes.
  VoteDataMap vote_data_map_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_MAX_VOTE_AGGREGATOR_H_
