// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/extension_service_worker_voter.h"

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const WorkerNode* worker_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             worker_node->GetGraph())
      ->GetExecutionContextForWorkerNode(worker_node);
}

// Returns a vote with the appropriate priority depending on if the worker
// is an extension service worker.
Vote GetVote(const WorkerNode* worker_node) {
  // Ideally we could query the extension to at least see if it has
  // the ability to use the more UI affecting APIs (e.g.
  // declarativeNetRequest, omnibox, input.ime (ChromeOS)) and only
  // set USER_BLOCKING on those extension service workers. But since
  // API access can change at runtime that gets fairly complicated so
  // for now set `USER_BLOCKING` so all use-cases will be covered.
  base::TaskPriority priority =
      worker_node->GetOrigin().GetURL().SchemeIs("chrome-extension")
          ? base::TaskPriority::USER_VISIBLE
          : base::TaskPriority::LOWEST;
  return Vote(priority, ExtensionServiceWorkerVoter::kPriorityReason);
}

}  // namespace

const char ExtensionServiceWorkerVoter::kPriorityReason[] =
    "Extension service worker.";

ExtensionServiceWorkerVoter::ExtensionServiceWorkerVoter() = default;
ExtensionServiceWorkerVoter::~ExtensionServiceWorkerVoter() = default;

void ExtensionServiceWorkerVoter::InitializeOnGraph(
    Graph* graph,
    VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddWorkerNodeObserver(this);
}

void ExtensionServiceWorkerVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveWorkerNodeObserver(this);
  voting_channel_.Reset();
}

void ExtensionServiceWorkerVoter::OnBeforeWorkerNodeAdded(
    const WorkerNode* worker_node,
    const ProcessNode* pending_process_node) {
  voting_channel_.SubmitVote(GetExecutionContext(worker_node),
                             GetVote(worker_node));
}

void ExtensionServiceWorkerVoter::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(worker_node));
}

}  // namespace performance_manager::execution_context_priority
