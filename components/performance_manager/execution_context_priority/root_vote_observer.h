// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_ROOT_VOTE_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_ROOT_VOTE_OBSERVER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

namespace performance_manager {
namespace execution_context_priority {

// The RootVoteObserver acts as the root node of a hierarchy of execution
// context priority voters. It is responsible for taking aggregated votes and
// applying them to the actual nodes in a graph.
class RootVoteObserver : public VoteObserver {
 public:
  RootVoteObserver();
  ~RootVoteObserver() override;

  RootVoteObserver(const RootVoteObserver&) = delete;
  RootVoteObserver& operator=(const RootVoteObserver&) = delete;

  // Issues a voting channel (registers the sole incoming voter).
  VotingChannel GetVotingChannel();

 protected:
  // VoteObserver implementation:
  void OnVoteSubmitted(VoterId voter_id,
                       const ExecutionContext* execution_context,
                       const Vote& vote) override;
  void OnVoteChanged(VoterId voter_id,
                     const ExecutionContext* context,
                     const Vote& new_vote) override;
  void OnVoteInvalidated(VoterId voter_id,
                         const ExecutionContext* context) override;

  // Provides the VotingChannel to our input voter.
  VotingChannelFactory voting_channel_factory_{this};

  // The ID of the only voting channel we've vended.
  VoterId voter_id_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_ROOT_VOTE_OBSERVER_H_
