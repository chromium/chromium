// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_INHERIT_CLIENT_PRIORITY_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_INHERIT_CLIENT_PRIORITY_VOTER_H_

#include "components/performance_manager/execution_context_priority/max_vote_aggregator.h"
#include "components/performance_manager/execution_context_priority/voter_base.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/worker_node.h"

namespace performance_manager {
namespace execution_context_priority {

// This voter ensures the priority of a client is inherited by its children
// workers.
class InheritClientPriorityVoter : public VoterBase,
                                   public FrameNode::ObserverDefaultImpl,
                                   public WorkerNode::ObserverDefaultImpl {
 public:
  static const char kPriorityInheritedReason[];

  explicit InheritClientPriorityVoter(VotingChannel voting_channel);
  ~InheritClientPriorityVoter() override;

  InheritClientPriorityVoter(const InheritClientPriorityVoter&) = delete;
  InheritClientPriorityVoter& operator=(const InheritClientPriorityVoter&) =
      delete;

  // VoterBase:
  void InitializeOnGraph(Graph* graph) override;
  void TearDownOnGraph(Graph* graph) override;

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) override;

  // WorkerNodeObserver:
  void OnWorkerNodeAdded(const WorkerNode* worker_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;
  void OnClientFrameAdded(const WorkerNode* worker_node,
                          const FrameNode* client_frame_node) override;
  void OnBeforeClientFrameRemoved(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) override;
  void OnClientWorkerAdded(const WorkerNode* worker_node,
                           const WorkerNode* client_worker_node) override;
  void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) override;
  void OnPriorityAndReasonChanged(
      const WorkerNode* worker_node,
      const PriorityAndReason& previous_value) override;

  VoterId voter_id() const { return voter_id_; }

 private:
  void OnExecutionContextAdded(const ExecutionContext* execution_context);
  void OnBeforeExecutionContextRemoved(
      const ExecutionContext* execution_context);
  void OnPriorityAndReasonChanged(const ExecutionContext* execution_context,
                                  const PriorityAndReason& previous_value);

  // Aggregates the votes from multiple clients of the same child worker.
  MaxVoteAggregator max_vote_aggregator_;

  VoterId voter_id_;

  // Each frame or worker gets a voting channel to cast votes for its children.
  base::flat_map<const ExecutionContext*, OptionalVotingChannel>
      voting_channels_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_INHERIT_CLIENT_PRIORITY_VOTER_H_
