// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_VOTING_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_VOTING_H_

#include "components/performance_manager/public/voting/voting.h"

#include <map>
#include <vector>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace voting {
namespace test {

// A dummy consumer that simply maintains a list of all submitted votes and
// doesn't explicitly clean them up. New votes are continuously pushed back to
// the end of |votes_|.
template <class VoteImpl>
class DummyVoteConsumer : public VoteConsumer<VoteImpl> {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using AcceptedVote = AcceptedVote<VoteImpl>;
  using VotingChannel = VotingChannel<VoteImpl>;

  DummyVoteConsumer();
  ~DummyVoteConsumer() override;
  DummyVoteConsumer(const DummyVoteConsumer& rhs) = delete;
  DummyVoteConsumer& operator=(const DummyVoteConsumer& rhs) = delete;

  // VoteConsumer implementation:
  VoteReceipt<VoteImpl> SubmitVote(util::PassKey<VotingChannel>,
                                   voting::VoterId<VoteImpl> voter_id,
                                   const ContextType* context,
                                   const VoteImpl& vote) override;
  void ChangeVote(util::PassKey<AcceptedVote>,
                  AcceptedVote* old_vote,
                  const VoteImpl& new_vote) override;
  void VoteInvalidated(util::PassKey<AcceptedVote>,
                       AcceptedVote* vote) override;

  void ExpectInvalidVote(size_t index);

  // Checks that the vote at position |index| is valid, and has the
  // corresponding |voter|, |context| and |vote_value|. If |reason| is
  // non-null then it will be validated as well.
  void ExpectValidVote(size_t index,
                       voting::VoterId<VoteImpl> voter_id,
                       const typename VoteImpl::ContextType* context,
                       typename VoteImpl::VoteType vote_value,
                       const char* reason = nullptr);

  VotingChannelFactory<VoteImpl> voting_channel_factory_;
  std::vector<AcceptedVote> votes_;
  size_t valid_vote_count_ = 0;
};

// A dummy voter that allows emitting votes and tracking their receipts.
template <class VoteImpl>
class DummyVoter {
 public:
  static constexpr char kReason[] = "dummmy reason";

  DummyVoter();
  ~DummyVoter();
  DummyVoter(const DummyVoter& rhs) = delete;
  DummyVoter& operator=(const DummyVoter& rhs) = delete;

  void SetVotingChannel(VotingChannel<VoteImpl>&& voting_channel);

  // Causes the voter to emit a vote for the given |context| and with
  // the given |vote_value|. The receipt is pushed back onto |receipts_|.
  void EmitVote(const typename VoteImpl::ContextType* context,
                typename VoteImpl::VoteType vote_value,
                const char* reason = kReason);

  VotingChannel<VoteImpl> voting_channel_;
  std::vector<VoteReceipt<VoteImpl>> receipts_;
};

template <class VoteImpl>
class DummyVoteObserver : public VoteObserver<VoteImpl> {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using VoteConsumerDefaultImpl = VoteConsumerDefaultImpl<VoteImpl>;

  DummyVoteObserver();
  ~DummyVoteObserver() override;

  // VoteObserver:
  void OnVoteSubmitted(voting::VoterId<VoteImpl> voter_id,
                       const ContextType* context,
                       const VoteImpl& vote) override;
  void OnVoteChanged(voting::VoterId<VoteImpl> voter_id,
                     const ContextType* context,
                     const VoteImpl& new_vote) override;
  void OnVoteInvalidated(voting::VoterId<VoteImpl> voter_id,
                         const ContextType* context) override;

  bool HasVote(voting::VoterId<VoteImpl> voter_id,
               const typename VoteImpl::ContextType* context,
               typename VoteImpl::VoteType vote_value,
               const char* reason = nullptr);

  VoteConsumerDefaultImpl vote_consumer_default_impl_{this};
  std::map<voting::VoterId<VoteImpl>, std::map<const ContextType*, VoteImpl>>
      votes_by_voter_id_;
};

// IMPLEMENTATION

template <class VoteImpl>
DummyVoteConsumer<VoteImpl>::DummyVoteConsumer()
    : voting_channel_factory_(this) {}

template <class VoteImpl>
DummyVoteConsumer<VoteImpl>::~DummyVoteConsumer() = default;

template <class VoteImpl>
VoteReceipt<VoteImpl> DummyVoteConsumer<VoteImpl>::SubmitVote(
    util::PassKey<VotingChannel>,
    voting::VoterId<VoteImpl> voter_id,
    const ContextType* context,
    const VoteImpl& vote) {
  // Accept the vote.
  votes_.emplace_back(AcceptedVote(this, voter_id, context, vote));
  EXPECT_FALSE(votes_.back().HasReceipt());
  EXPECT_TRUE(votes_.back().IsValid());
  ++valid_vote_count_;
  EXPECT_LE(valid_vote_count_, votes_.size());

  // Issue a receipt.
  auto receipt = votes_.back().IssueReceipt();
  EXPECT_TRUE(votes_.back().HasReceipt());
  EXPECT_TRUE(votes_.back().IsValid());
  return receipt;
}

template <class VoteImpl>
void DummyVoteConsumer<VoteImpl>::ChangeVote(util::PassKey<AcceptedVote>,
                                             AcceptedVote* old_vote,
                                             const VoteImpl& new_vote) {
  // We should own this vote and it should be valid.
  EXPECT_LE(votes_.data(), old_vote);
  EXPECT_LT(old_vote, votes_.data() + votes_.size());
  EXPECT_TRUE(old_vote->IsValid());
  EXPECT_LT(0u, valid_vote_count_);

  // Update the vote in-place.
  old_vote->UpdateVote(new_vote);
}

template <class VoteImpl>
void DummyVoteConsumer<VoteImpl>::VoteInvalidated(util::PassKey<AcceptedVote>,
                                                  AcceptedVote* vote) {
  // We should own this vote.
  EXPECT_LE(votes_.data(), vote);
  EXPECT_LT(vote, votes_.data() + votes_.size());
  EXPECT_FALSE(vote->IsValid());
  EXPECT_LT(0u, valid_vote_count_);
  --valid_vote_count_;
}

template <class VoteImpl>
void DummyVoteConsumer<VoteImpl>::ExpectInvalidVote(size_t index) {
  EXPECT_LT(index, votes_.size());
  const auto& accepted_vote = votes_[index];
  EXPECT_EQ(this, accepted_vote.consumer());
  EXPECT_FALSE(accepted_vote.IsValid());
}

template <class VoteImpl>
void DummyVoteConsumer<VoteImpl>::ExpectValidVote(
    size_t index,
    voting::VoterId<VoteImpl> voter_id,
    const typename VoteImpl::ContextType* context,
    typename VoteImpl::VoteType vote_value,
    const char* reason) {
  EXPECT_LT(index, votes_.size());
  const auto& accepted_vote = votes_[index];
  EXPECT_EQ(this, accepted_vote.consumer());
  EXPECT_TRUE(accepted_vote.IsValid());
  EXPECT_EQ(voter_id, accepted_vote.voter_id());
  EXPECT_EQ(context, accepted_vote.context());
  const auto& vote = accepted_vote.vote();
  EXPECT_EQ(vote_value, vote.value());
  EXPECT_TRUE(vote.reason());
  if (reason)
    EXPECT_EQ(reason, vote.reason());
}

template <class VoteImpl>
constexpr char DummyVoter<VoteImpl>::kReason[];

template <class VoteImpl>
DummyVoter<VoteImpl>::DummyVoter() = default;
template <class VoteImpl>
DummyVoter<VoteImpl>::~DummyVoter() = default;

template <class VoteImpl>
void DummyVoter<VoteImpl>::SetVotingChannel(
    VotingChannel<VoteImpl>&& voting_channel) {
  voting_channel_ = std::move(voting_channel);
}

template <class VoteImpl>
void DummyVoter<VoteImpl>::EmitVote(
    const typename VoteImpl::ContextType* context,
    typename VoteImpl::VoteType vote_value,
    const char* reason) {
  EXPECT_TRUE(voting_channel_.IsValid());
  receipts_.emplace_back(
      voting_channel_.SubmitVote(context, VoteImpl(vote_value, reason)));
}

template <class VoteImpl>
DummyVoteObserver<VoteImpl>::DummyVoteObserver() = default;

template <class VoteImpl>
DummyVoteObserver<VoteImpl>::~DummyVoteObserver() = default;

template <class VoteImpl>
void DummyVoteObserver<VoteImpl>::OnVoteSubmitted(VoterId<VoteImpl> voter_id,
                                                  const ContextType* context,
                                                  const VoteImpl& vote) {
  bool inserted = votes_by_voter_id_[voter_id].emplace(context, vote).second;
  DCHECK(inserted);
}

template <class VoteImpl>
void DummyVoteObserver<VoteImpl>::OnVoteChanged(VoterId<VoteImpl> voter_id,
                                                const ContextType* context,
                                                const VoteImpl& new_vote) {
  auto it = votes_by_voter_id_[voter_id].find(context);
  DCHECK(it != votes_by_voter_id_[voter_id].end());
  it->second = new_vote;
}

template <class VoteImpl>
void DummyVoteObserver<VoteImpl>::OnVoteInvalidated(
    VoterId<VoteImpl> voter_id,
    const ContextType* context) {
  auto it = votes_by_voter_id_.find(voter_id);
  DCHECK(it != votes_by_voter_id_.end());

  std::map<const ContextType*, VoteImpl>& votes = it->second;
  size_t removed = votes.erase(context);
  DCHECK_EQ(removed, 1u);

  if (votes.empty())
    votes_by_voter_id_.erase(it);
}

template <class VoteImpl>
bool DummyVoteObserver<VoteImpl>::HasVote(
    voting::VoterId<VoteImpl> voter_id,
    const typename VoteImpl::ContextType* context,
    typename VoteImpl::VoteType vote_value,
    const char* reason) {
  if (!base::Contains(votes_by_voter_id_, voter_id))
    return false;

  std::map<const ContextType*, VoteImpl>& votes = votes_by_voter_id_[voter_id];

  if (!base::Contains(votes, context))
    return false;

  return votes[context] == VoteImpl(vote_value, reason);
}

}  // namespace test
}  // namespace voting
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_VOTING_H_
