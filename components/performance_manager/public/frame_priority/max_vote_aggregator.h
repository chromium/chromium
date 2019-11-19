// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_MAX_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_MAX_VOTE_AGGREGATOR_H_

#include <map>
#include <utility>

#include "base/containers/intrusive_heap.h"
#include "components/performance_manager/public/frame_priority/frame_priority.h"

namespace performance_manager {
namespace frame_priority {

// Aggregator that allows votes from an arbitrary number of voters, and forwards
// the maximum vote for each frame. The upstream voting channel must be set
// before any votes are submitted to this aggregator. New voting channels may
// continue to be issued at any time during its lifetime, however.
class MaxVoteAggregator : public VoteConsumer {
 public:
  MaxVoteAggregator();
  ~MaxVoteAggregator() override;

  // Issues a voting channel (effectively registered a voter).
  VotingChannel GetVotingChannel();

  // Sets the upstream voting channel. Should only be called once.
  void SetUpstreamVotingChannel(VotingChannel&& channel);

 protected:
  // VoteConsumer implementation:
  VoteReceipt SubmitVote(VoterId voter_id, const Vote& vote) override;
  VoteReceipt ChangeVote(VoteReceipt receipt,
                         AcceptedVote* old_vote,
                         const Vote& new_vote) override;
  void VoteInvalidated(AcceptedVote* vote) override;

 private:
  friend class MaxVoteAggregatorTestAccess;

  // A StampedVote is an AcceptedVote with a serial number that can be used to
  // order votes by the order in which they were received. This ensures that
  // votes upstreamed by this aggregator remain as stable as possible.
  struct StampedVote {
    StampedVote() = default;
    StampedVote(AcceptedVote&& vote, uint32_t vote_id)
        : vote(std::move(vote)), vote_id(vote_id) {}
    StampedVote(StampedVote&&) = default;
    StampedVote(const StampedVote&) = delete;
    ~StampedVote() = default;

    StampedVote& operator=(StampedVote&&) = default;
    StampedVote& operator=(const StampedVote&) = delete;

    bool operator<(const StampedVote& rhs) const {
      if (vote.vote().priority() != rhs.vote.vote().priority())
        return vote.vote().priority() < rhs.vote.vote().priority();
      // Higher |vote_id| values are of lower priority.
      return vote_id > rhs.vote_id;
    }

    // IntrusiveHeap contract. We actually don't need HeapHandles, as we already
    // know the positions of the elements in the heap directly, as they are
    // tracked with explicit back pointers.
    void SetHeapHandle(base::HeapHandle) {}
    void ClearHeapHandle() {}
    base::HeapHandle GetHeapHandle() const {
      return base::HeapHandle::Invalid();
    }

    AcceptedVote vote;
    uint32_t vote_id = 0;
  };

  // The collection of votes for a single frame. This is move-only because all
  // of its members are move-only. Internally it houses the collection of all
  // votes associated with a frame as max-heap, and a receipt for the vote that
  // has been upstreamed.
  class VoteData {
   public:
    VoteData();
    VoteData(const VoteData& rhs) = delete;
    VoteData(VoteData&& rhs);
    VoteData& operator=(const VoteData& rhs) = delete;
    VoteData& operator=(VoteData&& rhs);
    ~VoteData();

    // Adds a vote. Returns true if a new upstream vote is needed.
    bool AddVote(AcceptedVote&& vote, uint32_t vote_id);

    // Updates the vote from its given index to a new index. Returns true if the
    // root was disturbed and a new upstream vote is needed.
    bool UpdateVote(size_t index, uint32_t vote_id);

    // Removes the vote at the provided index. Returns true if the root was
    // disturbed and a new upstream vote is needed.
    bool RemoveVote(size_t index);

    // Gets the index of the given vote.
    size_t GetVoteIndex(AcceptedVote* vote);

    // Upstreams the vote for this vote data, using the given voting |channel|.
    void UpstreamVote(VotingChannel* channel);

    // Returns the number of votes in this structure.
    size_t GetSize() const { return votes_.size(); }

    // Returns true if this VoteData is empty.
    bool IsEmpty() const { return votes_.empty(); }

    AcceptedVote& GetVoteForTesting(size_t index) {
      return const_cast<AcceptedVote&>(votes_[index].vote);
    }

   private:
    base::IntrusiveHeap<StampedVote> votes_;

    // The receipt for the vote we've upstreamed.
    VoteReceipt receipt_;
  };

  using VoteDataMap = std::map<const FrameNode*, VoteData>;

  // Looks up the VoteData associated with the provided |vote|. The data is
  // expected to already exist (enforced by a DCHECK).
  VoteDataMap::iterator GetVoteData(AcceptedVote* vote);

  // Our channel for upstreaming our votes.
  VotingChannel channel_;

  // Our VotingChannelFactory for providing VotingChannels to our input voters.
  VotingChannelFactory factory_;

  // The next StampedVote ID to use.
  uint32_t next_vote_id_;

  // Received votes, plus all of the upstreamed votes.
  VoteDataMap vote_data_map_;

  DISALLOW_COPY_AND_ASSIGN(MaxVoteAggregator);
};

}  // namespace frame_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_MAX_VOTE_AGGREGATOR_H_
