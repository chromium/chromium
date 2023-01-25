// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_vote_aggregator.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {
namespace freezing {

namespace {
const char kDescriberName[] = "FreezingVoteAggregator";
}

FreezingVoteAggregator::FreezingVoteAggregator() = default;

FreezingVoteAggregator::~FreezingVoteAggregator() = default;

FreezingVotingChannel FreezingVoteAggregator::GetVotingChannel() {
  return freezing_voting_channel_factory_.BuildVotingChannel();
}

void FreezingVoteAggregator::SetUpstreamVotingChannel(
    FreezingVotingChannel&& channel) {
  channel_ = std::move(channel);
}

void FreezingVoteAggregator::OnVoteSubmitted(FreezingVoterId voter_id,
                                             const PageNode* page_node,
                                             const FreezingVote& vote) {
  DCHECK(channel_.IsValid());

  // Create the VoteData for this page node, if necessary.
  auto& vote_data = vote_data_map_[page_node];

  // Remember the previous chosen vote before adding the new vote. There
  // could be none if this is the first vote submitted for |page_node|.
  absl::optional<FreezingVoteValue> old_chosen_vote_value;
  if (!vote_data.IsEmpty())
    old_chosen_vote_value = vote_data.GetChosenVote().value();

  vote_data.AddVote(voter_id, vote);

  // If there was no previous chosen vote, the vote must be submitted.
  if (!old_chosen_vote_value) {
    channel_.SubmitVote(page_node, vote);
    return;
  }

  // Since there is a previous chosen vote, it must be modified if the chosen
  // vote changed.
  const FreezingVote new_chosen_vote = vote_data.GetChosenVote();
  if (*old_chosen_vote_value != new_chosen_vote.value())
    channel_.ChangeVote(page_node, new_chosen_vote);
}

void FreezingVoteAggregator::OnVoteChanged(FreezingVoterId voter_id,
                                           const PageNode* page_node,
                                           const FreezingVote& new_vote) {
  // The vote data for this page node is guaranteed to exist.
  auto& vote_data = GetVoteData(page_node)->second;

  // Remember the previous chosen vote before updating the vote for this
  // |voter_id|.
  const FreezingVoteValue old_chosen_vote_value =
      vote_data.GetChosenVote().value();

  vote_data.UpdateVote(voter_id, new_vote);

  // If the chosen vote changed, the upstream vote must also be changed.
  const FreezingVote new_chosen_vote = vote_data.GetChosenVote();
  if (old_chosen_vote_value != new_chosen_vote.value())
    channel_.ChangeVote(page_node, new_chosen_vote);
}

void FreezingVoteAggregator::OnVoteInvalidated(FreezingVoterId voter_id,
                                               const PageNode* page_node) {
  // The VoteData for this page node is guaranteed to exist.
  auto it = GetVoteData(page_node);
  auto& vote_data = it->second;

  // Remember the previous chosen vote before removing the vote for this
  // |voter_id|.
  const FreezingVoteValue old_chosen_vote_value =
      vote_data.GetChosenVote().value();

  vote_data.RemoveVote(voter_id);

  // In case the last vote for |page_node| was invalidated, the upstream vote
  // must also be invalidated.
  if (vote_data.IsEmpty()) {
    channel_.InvalidateVote(page_node);

    // Clean up the VoteData for |page_node| since it is empty.
    vote_data_map_.erase(it);
    return;
  }

  // If the chosen vote changed, the upstream vote must also be changed.
  const FreezingVote new_chosen_vote = vote_data.GetChosenVote();
  if (old_chosen_vote_value != new_chosen_vote.value())
    channel_.ChangeVote(page_node, new_chosen_vote);
}

void FreezingVoteAggregator::RegisterNodeDataDescriber(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void FreezingVoteAggregator::UnregisterNodeDataDescriber(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value FreezingVoteAggregator::DescribePageNodeData(
    const PageNode* node) const {
  auto votes_for_page = vote_data_map_.find(node);
  if (votes_for_page == vote_data_map_.end())
    return base::Value();

  base::Value::Dict ret;
  votes_for_page->second.DescribeVotes(ret);
  return base::Value(std::move(ret));
}

FreezingVoteAggregator::FreezingVoteData::FreezingVoteData() = default;
FreezingVoteAggregator::FreezingVoteData::FreezingVoteData(FreezingVoteData&&) =
    default;
FreezingVoteAggregator::FreezingVoteData&
FreezingVoteAggregator::FreezingVoteData::operator=(
    FreezingVoteAggregator::FreezingVoteData&& rhs) = default;
FreezingVoteAggregator::FreezingVoteData::~FreezingVoteData() = default;

void FreezingVoteAggregator::FreezingVoteData::AddVote(
    FreezingVoterId voter_id,
    const FreezingVote& vote) {
  AddVoteToDeque(voter_id, vote);
}

void FreezingVoteAggregator::FreezingVoteData::UpdateVote(
    FreezingVoterId voter_id,
    const FreezingVote& new_vote) {
  // The vote is removed from the deque and then re-inserted.
  auto it = FindVote(voter_id);
  DCHECK(it != votes_.end());
  votes_.erase(it);

  AddVoteToDeque(voter_id, new_vote);
}

void FreezingVoteAggregator::FreezingVoteData::RemoveVote(
    FreezingVoterId voter_id) {
  votes_.erase(FindVote(voter_id));
}

const FreezingVote& FreezingVoteAggregator::FreezingVoteData::GetChosenVote() {
  DCHECK(!IsEmpty());
  // The set of votes is ordered and the first one in the set is the one that
  // should be sent to the consumer.
  return votes_.begin()->second;
}

void FreezingVoteAggregator::FreezingVoteData::DescribeVotes(
    base::Value::Dict& ret) const {
  size_t i = 0;
  for (const auto& it : votes_) {
    ret.Set(base::StringPrintf("Vote %zu (%s)", i++,
                               FreezingVoteValueToString(it.second.value())),
            it.second.reason());
  }
}

FreezingVoteAggregator::FreezingVoteData::VotesDeque::iterator
FreezingVoteAggregator::FreezingVoteData::FindVote(FreezingVoterId voter_id) {
  // TODO(sebmarchand): Consider doing a reverse search for kCanFreeze votes and
  // a normal one for kCannotFreeze votes.

  auto it =
      base::ranges::find(votes_, voter_id, &VotesDeque::value_type::first);
  DCHECK(it != votes_.end());
  return it;
}

void FreezingVoteAggregator::FreezingVoteData::AddVoteToDeque(
    FreezingVoterId voter_id,
    const FreezingVote& vote) {
  DCHECK(!base::Contains(votes_, voter_id, &VotesDeque::value_type::first));
  if (vote.value() == FreezingVoteValue::kCannotFreeze) {
    votes_.emplace_front(voter_id, vote);
  } else {
    votes_.emplace_back(voter_id, vote);
  }
}

FreezingVoteAggregator::VoteDataMap::iterator
FreezingVoteAggregator::GetVoteData(const PageNode* page_node) {
  auto it = vote_data_map_.find(page_node);
  DCHECK(it != vote_data_map_.end());
  return it;
}

}  // namespace freezing
}  // namespace performance_manager
