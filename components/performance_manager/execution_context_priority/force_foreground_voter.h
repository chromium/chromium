// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FORCE_FOREGROUND_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FORCE_FOREGROUND_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/voting/voting.h"

namespace performance_manager::execution_context_priority {

// This voter boosts the priority of all frames and workers, unconditionally.
class ForceForegroundVoter : public PriorityVoter,
                             public FrameNodeObserver,
                             public WorkerNodeObserver,
                             public GraphRegisteredImpl<ForceForegroundVoter> {
 public:
  static const char kForceForegroundReason[];

  ForceForegroundVoter();
  ~ForceForegroundVoter() override;

  ForceForegroundVoter(const ForceForegroundVoter&) = delete;
  ForceForegroundVoter& operator=(const ForceForegroundVoter&) = delete;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;

  // WorkerNodeObserver:
  void OnBeforeWorkerNodeAdded(
      const WorkerNode* worker_node,
      const ProcessNode* pending_process_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  void AddVoteForExecutionContext(
      const execution_context::ExecutionContext* execution_context);
  void RemoveVoteForExecutionContext(
      const execution_context::ExecutionContext* execution_context);

  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_FORCE_FOREGROUND_VOTER_H_
