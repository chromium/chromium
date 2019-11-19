// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_FRAME_PRIORITY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_FRAME_PRIORITY_H_

#include "components/performance_manager/public/frame_priority/frame_priority.h"

#include "base/macros.h"

namespace performance_manager {
namespace frame_priority {
namespace test {

// A dummy consumer that simply maintains a list of all submitted votes and
// doesn't explicitly clean them up. New votes are continuously pushed back to
// the end of |votes_|.
class DummyVoteConsumer : public VoteConsumer {
 public:
  DummyVoteConsumer();
  ~DummyVoteConsumer() override;

  // VoteConsumer implementation:
  VoteReceipt SubmitVote(VoterId voter_id, const Vote& vote) override;
  VoteReceipt ChangeVote(VoteReceipt receipt,
                         AcceptedVote* old_vote,
                         const Vote& new_vote) override;
  void VoteInvalidated(AcceptedVote* vote) override;

  void ExpectInvalidVote(size_t index);

  // Checks that the vote at position |index| is valid, and has the
  // corresponding |voter|, |frame_node| and |priority|. If |reason| is non-null
  // then it will be validated as well.
  void ExpectValidVote(size_t index,
                       VoterId voter_id,
                       const FrameNode* frame_node,
                       base::TaskPriority priority,
                       const char* reason = nullptr);

  VotingChannelFactory voting_channel_factory_;
  std::vector<AcceptedVote> votes_;
  size_t valid_vote_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyVoteConsumer);
};

// A dummy voter that allows emitting votes and tracking their receipts.
class DummyVoter {
 public:
  static const char kReason[];

  DummyVoter();
  ~DummyVoter();

  void SetVotingChannel(VotingChannel&& voting_channel);

  // Causes the voter to emit a vote for the given |frame_node| and with the
  // given |priority|. The receipt is pushed back onto |receipts_|.
  void EmitVote(const FrameNode* frame_node,
                base::TaskPriority priority = base::TaskPriority::LOWEST,
                const char* reason = kReason);

  VotingChannel voting_channel_;
  std::vector<VoteReceipt> receipts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyVoter);
};

}  // namespace test
}  // namespace frame_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_FRAME_PRIORITY_H_
