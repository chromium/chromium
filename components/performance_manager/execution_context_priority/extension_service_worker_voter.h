// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXTENSION_SERVICE_WORKER_VOTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXTENSION_SERVICE_WORKER_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/worker_node.h"

namespace performance_manager::execution_context_priority {

// This voter ensures the correct task priority (and indirectly renderer
// process priority) of extension processes that host extension service
// workers.
class ExtensionServiceWorkerVoter : public PriorityVoter,
                                    public WorkerNodeObserver {
 public:
  ExtensionServiceWorkerVoter();
  ~ExtensionServiceWorkerVoter() override;

  ExtensionServiceWorkerVoter(const ExtensionServiceWorkerVoter&) = delete;
  ExtensionServiceWorkerVoter& operator=(const ExtensionServiceWorkerVoter&) =
      delete;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // WorkerNodeObserver:
  void OnBeforeWorkerNodeAdded(
      const WorkerNode* worker_node,
      const ProcessNode* pending_process_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

  static const char kPriorityReason[];

 private:
  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXTENSION_SERVICE_WORKER_VOTER_H_
