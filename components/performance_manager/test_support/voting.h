// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_VOTING_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_VOTING_H_

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/not_fatal_until.h"
#include "components/performance_manager/public/voting/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace voting {
namespace test {

// A dummy observer that simply maintains a list of all submitted votes.
template <class VoteImpl>
class DummyVoteObserver : public VoteObserver<VoteImpl> {
 public:
  using ContextType = typename VoteImpl::ContextType;

  DummyVoteObserver();
  ~DummyVoteObserver() override;

  VotingChannel<VoteImpl> BuildVotingChannel();

  size_t GetVoteCount() const;
  size_t GetVoteCountForContext(
      const typename VoteImpl::ContextType* context) const;
  size_t GetVoteCountForVoterId(voting::VoterId<VoteImpl> voter_id) const;

  bool HasVote(voting::VoterId<VoteImpl> voter_id,
               const typename VoteImpl::ContextType* context) const;

  bool HasVote(voting::VoterId<VoteImpl> voter_id,
               const typename VoteImpl::ContextType* context,
               const VoteImpl& vote) const;

  bool HasVote(voting::VoterId<VoteImpl> voter_id,
               const typename VoteImpl::ContextType* context,
               typename VoteImpl::VoteType vote_value,
               const char* reason = nullptr) const;

 protected:
  // VoteObserver:
  void OnVoteSubmitted(voting::VoterId<VoteImpl> voter_id,
                       const ContextType* context,
                       const VoteImpl& vote) override;
  void OnVoteChanged(voting::VoterId<VoteImpl> voter_id,
                     const ContextType* context,
                     const VoteImpl& new_vote) override;
  void OnVoteInvalidated(voting::VoterId<VoteImpl> voter_id,
                         const ContextType* context) override;

 private:
  VotingChannelFactory<VoteImpl> voting_channel_factory_{this};

  base::flat_map<voting::VoterId<VoteImpl>,
                 base::flat_map<const ContextType*, VoteImpl>>
      votes_by_voter_id_;
};

template <class VoteImpl>
DummyVoteObserver<VoteImpl>::DummyVoteObserver() = default;

template <class VoteImpl>
DummyVoteObserver<VoteImpl>::~DummyVoteObserver() = default;

template <class VoteImpl>
VotingChannel<VoteImpl> DummyVoteObserver<VoteImpl>::BuildVotingChannel() {
  return voting_channel_factory_.BuildVotingChannel();
}

template <class VoteImpl>
size_t DummyVoteObserver<VoteImpl>::GetVoteCount() const {
  size_t vote_count = 0;
  for (const auto& votes : votes_by_voter_id_) {
    vote_count += votes.second.size();
  }
  return vote_count;
}

template <class VoteImpl>
size_t DummyVoteObserver<VoteImpl>::GetVoteCountForVoterId(
    voting::VoterId<VoteImpl> voter_id) const {
  auto it = votes_by_voter_id_.find(voter_id);
  if (it == votes_by_voter_id_.end())
    return 0;

  return it->second.size();
}

template <class VoteImpl>
size_t DummyVoteObserver<VoteImpl>::GetVoteCountForContext(
    const typename VoteImpl::ContextType* context) const {
  size_t vote_count = 0;
  for (const auto& votes : votes_by_voter_id_) {
    vote_count += votes.second.count(context);
  }
  return vote_count;
}

template <class VoteImpl>
bool DummyVoteObserver<VoteImpl>::HasVote(
    voting::VoterId<VoteImpl> voter_id,
    const typename VoteImpl::ContextType* context) const {
  auto votes_it = votes_by_voter_id_.find(voter_id);
  if (votes_it == votes_by_voter_id_.end())
    return false;

  const auto& votes = votes_it->second;

  return base::Contains(votes, context);
}

template <class VoteImpl>
bool DummyVoteObserver<VoteImpl>::HasVote(
    voting::VoterId<VoteImpl> voter_id,
    const typename VoteImpl::ContextType* context,
    const VoteImpl& vote) const {
  auto votes_it = votes_by_voter_id_.find(voter_id);
  if (votes_it == votes_by_voter_id_.end())
    return false;

  const auto& votes = votes_it->second;

  auto vote_it = votes.find(context);
  if (vote_it == votes.end())
    return false;

  return vote_it->second == vote;
}

template <class VoteImpl>
bool DummyVoteObserver<VoteImpl>::HasVote(
    voting::VoterId<VoteImpl> voter_id,
    const typename VoteImpl::ContextType* context,
    typename VoteImpl::VoteType vote_value,
    const char* reason) const {
  return HasVote(voter_id, context, VoteImpl(vote_value, reason));
}

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
  CHECK(it != votes_by_voter_id_[voter_id].end(), base::NotFatalUntil::M130);
  it->second = new_vote;
}

template <class VoteImpl>
void DummyVoteObserver<VoteImpl>::OnVoteInvalidated(
    VoterId<VoteImpl> voter_id,
    const ContextType* context) {
  auto it = votes_by_voter_id_.find(voter_id);
  CHECK(it != votes_by_voter_id_.end(), base::NotFatalUntil::M130);

  base::flat_map<const ContextType*, VoteImpl>& votes = it->second;
  size_t removed = votes.erase(context);
  DCHECK_EQ(removed, 1u);

  if (votes.empty())
    votes_by_voter_id_.erase(it);
}

}  // namespace test
}  // namespace voting
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_VOTING_H_
