// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/force_foreground_voter_for_urls.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/url_matcher/url_util.h"

namespace performance_manager::execution_context_priority {

// static
const char ForceForegroundVoterForUrls::kForceForegroundReason[] =
    "Force foreground for URLs.";

ForceForegroundVoterForUrls::ForceForegroundVoterForUrls() = default;

ForceForegroundVoterForUrls::~ForceForegroundVoterForUrls() = default;

void ForceForegroundVoterForUrls::SetPatternsForProfile(
    const std::string& browser_context_id,
    const base::ListValue& patterns) {
  std::unique_ptr<url_matcher::URLMatcher>& entry =
      profiles_force_foreground_patterns_[browser_context_id];
  entry = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddAllowFiltersWithLimit(entry.get(), patterns);

  for (const auto* frame_node : graph_->GetAllFrameNodes()) {
    if (frame_node->GetPageNode()->GetBrowserContextID() ==
        browser_context_id) {
      if (ShouldBoost(browser_context_id, frame_node->GetURL())) {
        RequestForeground(frame_node);
      } else {
        ReleaseForeground(frame_node);
      }
    }
  }
  for (const auto* worker_node : graph_->GetAllWorkerNodes()) {
    if (worker_node->GetBrowserContextID() == browser_context_id) {
      if (ShouldBoost(browser_context_id, worker_node->GetURL())) {
        RequestForeground(worker_node);
      } else {
        ReleaseForeground(worker_node);
      }
    }
  }
}

void ForceForegroundVoterForUrls::ClearPatternsForProfile(
    const std::string& browser_context_id) {
  size_t removed =
      profiles_force_foreground_patterns_.erase(browser_context_id);
  CHECK_EQ(removed, 1u);

  for (const auto* frame_node : graph_->GetAllFrameNodes()) {
    if (frame_node->GetPageNode()->GetBrowserContextID() ==
        browser_context_id) {
      ReleaseForeground(frame_node);
    }
  }
  for (const auto* worker_node : graph_->GetAllWorkerNodes()) {
    if (worker_node->GetBrowserContextID() == browser_context_id) {
      ReleaseForeground(worker_node);
    }
  }
}

void ForceForegroundVoterForUrls::InitializeOnGraph(
    Graph* graph,
    VotingChannel voting_channel) {
  CHECK(graph->HasOnlySystemNode());
  graph_ = graph;
  graph->RegisterObject(this);
  voting_channel_ = std::move(voting_channel);
  graph->AddFrameNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void ForceForegroundVoterForUrls::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemoveWorkerNodeObserver(this);
  voting_channel_.Reset();
  graph->UnregisterObject(this);
  graph_ = nullptr;
}

void ForceForegroundVoterForUrls::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  CHECK(frame_node->GetURL().is_empty());
}

void ForceForegroundVoterForUrls::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  ReleaseForeground(frame_node);
}

void ForceForegroundVoterForUrls::OnURLChanged(const FrameNode* frame_node,
                                               const GURL& previous_value) {
  if (ShouldBoost(frame_node->GetPageNode()->GetBrowserContextID(),
                  frame_node->GetURL())) {
    RequestForeground(frame_node);
  } else {
    ReleaseForeground(frame_node);
  }
}

void ForceForegroundVoterForUrls::OnBeforeWorkerNodeAdded(
    const WorkerNode* worker_node,
    const ProcessNode* pending_process_node) {
  CHECK(worker_node->GetURL().is_empty());
}

void ForceForegroundVoterForUrls::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  ReleaseForeground(worker_node);
}

void ForceForegroundVoterForUrls::OnFinalResponseURLDetermined(
    const WorkerNode* worker_node) {
  if (ShouldBoost(worker_node->GetBrowserContextID(), worker_node->GetURL())) {
    RequestForeground(worker_node);
  } else {
    ReleaseForeground(worker_node);
  }
}

void ForceForegroundVoterForUrls::RequestForeground(
    const FrameNode* frame_node) {
  RequestForeground(execution_context::ExecutionContext::From(frame_node));
}

void ForceForegroundVoterForUrls::RequestForeground(
    const WorkerNode* worker_node) {
  RequestForeground(execution_context::ExecutionContext::From(worker_node));
}

void ForceForegroundVoterForUrls::ReleaseForeground(
    const FrameNode* frame_node) {
  ReleaseForeground(execution_context::ExecutionContext::From(frame_node));
}

void ForceForegroundVoterForUrls::ReleaseForeground(
    const WorkerNode* worker_node) {
  ReleaseForeground(execution_context::ExecutionContext::From(worker_node));
}

void ForceForegroundVoterForUrls::RequestForeground(
    const execution_context::ExecutionContext* execution_context) {
  auto [_, inserted] = foregrounded_contexts_.insert(execution_context);
  if (inserted) {
    voting_channel_.SubmitVote(
        execution_context,
        Vote(base::Process::Priority::kUserBlocking, kForceForegroundReason));
  }
}

void ForceForegroundVoterForUrls::ReleaseForeground(
    const execution_context::ExecutionContext* execution_context) {
  size_t removed = foregrounded_contexts_.erase(execution_context);
  if (removed) {
    voting_channel_.InvalidateVote(execution_context);
  }
}

bool ForceForegroundVoterForUrls::ShouldBoost(
    const std::string& browser_context_id,
    const GURL& url) const {
  auto it = profiles_force_foreground_patterns_.find(browser_context_id);
  if (it == profiles_force_foreground_patterns_.end()) {
    return false;
  }
  return !it->second->MatchURL(url).empty();
}

}  // namespace performance_manager::execution_context_priority
