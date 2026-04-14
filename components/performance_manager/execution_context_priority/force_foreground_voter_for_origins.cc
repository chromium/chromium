// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/force_foreground_voter_for_origins.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/url_matcher/url_util.h"

namespace performance_manager::execution_context_priority {

// static
const char ForceForegroundVoterForOrigins::kForceForegroundReason[] =
    "Force foreground for origins.";

ForceForegroundVoterForOrigins::ForceForegroundVoterForOrigins() = default;

ForceForegroundVoterForOrigins::~ForceForegroundVoterForOrigins() = default;

void ForceForegroundVoterForOrigins::SetPatternsForProfile(
    const std::string& browser_context_id,
    const base::ListValue& patterns) {
  std::unique_ptr<url_matcher::URLMatcher>& entry =
      profiles_force_foreground_patterns_[browser_context_id];
  entry = std::make_unique<url_matcher::URLMatcher>();

  url_matcher::URLMatcherConditionSet::Vector all_conditions;
  base::MatcherStringPattern::ID id(0);

  for (const auto& pattern_value : patterns) {
    if (!pattern_value.is_string()) {
      continue;
    }
    std::string pattern = pattern_value.GetString();
    std::string scheme, host, path, query;
    bool match_subdomains;
    uint16_t port;
    if (url_matcher::util::FilterToComponents(
            pattern, &scheme, &host, &match_subdomains, &port, &path, &query)) {
      // Create a condition set that matches only the origin (no path/query).
      all_conditions.push_back(url_matcher::util::CreateConditionSet(
          entry.get(), ++id, scheme, host, match_subdomains, port,
          /*path=*/std::string(), /*query=*/std::string(), /*allow=*/true));
    }
  }

  entry->AddConditionSets(all_conditions);
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

void ForceForegroundVoterForOrigins::ClearPatternsForProfile(
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

void ForceForegroundVoterForOrigins::InitializeOnGraph(
    Graph* graph,
    VotingChannel voting_channel) {
  CHECK(graph->HasOnlySystemNode());
  graph_ = graph;
  graph->RegisterObject(this);
  voting_channel_ = std::move(voting_channel);
  graph->AddFrameNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void ForceForegroundVoterForOrigins::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemoveWorkerNodeObserver(this);
  voting_channel_.Reset();
  graph->UnregisterObject(this);
  graph_ = nullptr;
}

void ForceForegroundVoterForOrigins::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  CHECK(frame_node->GetURL().is_empty());
}

void ForceForegroundVoterForOrigins::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  ReleaseForeground(frame_node);
}

void ForceForegroundVoterForOrigins::OnURLChanged(const FrameNode* frame_node,
                                                  const GURL& previous_value) {
  if (ShouldBoost(frame_node->GetPageNode()->GetBrowserContextID(),
                  frame_node->GetURL())) {
    RequestForeground(frame_node);
  } else {
    ReleaseForeground(frame_node);
  }
}

void ForceForegroundVoterForOrigins::OnBeforeWorkerNodeAdded(
    const WorkerNode* worker_node,
    const ProcessNode* pending_process_node) {
  CHECK(worker_node->GetURL().is_empty());
}

void ForceForegroundVoterForOrigins::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  ReleaseForeground(worker_node);
}

void ForceForegroundVoterForOrigins::OnFinalResponseURLDetermined(
    const WorkerNode* worker_node) {
  if (ShouldBoost(worker_node->GetBrowserContextID(), worker_node->GetURL())) {
    RequestForeground(worker_node);
  } else {
    ReleaseForeground(worker_node);
  }
}

void ForceForegroundVoterForOrigins::RequestForeground(
    const FrameNode* frame_node) {
  RequestForeground(execution_context::ExecutionContext::From(frame_node));
}

void ForceForegroundVoterForOrigins::RequestForeground(
    const WorkerNode* worker_node) {
  RequestForeground(execution_context::ExecutionContext::From(worker_node));
}

void ForceForegroundVoterForOrigins::ReleaseForeground(
    const FrameNode* frame_node) {
  ReleaseForeground(execution_context::ExecutionContext::From(frame_node));
}

void ForceForegroundVoterForOrigins::ReleaseForeground(
    const WorkerNode* worker_node) {
  ReleaseForeground(execution_context::ExecutionContext::From(worker_node));
}

void ForceForegroundVoterForOrigins::RequestForeground(
    const execution_context::ExecutionContext* execution_context) {
  auto [_, inserted] = foregrounded_contexts_.insert(execution_context);
  if (inserted) {
    voting_channel_.SubmitVote(
        execution_context,
        Vote(base::Process::Priority::kUserBlocking, kForceForegroundReason));
  }
}

void ForceForegroundVoterForOrigins::ReleaseForeground(
    const execution_context::ExecutionContext* execution_context) {
  size_t removed = foregrounded_contexts_.erase(execution_context);
  if (removed) {
    voting_channel_.InvalidateVote(execution_context);
  }
}

bool ForceForegroundVoterForOrigins::ShouldBoost(
    const std::string& browser_context_id,
    const GURL& url) const {
  auto it = profiles_force_foreground_patterns_.find(browser_context_id);
  if (it == profiles_force_foreground_patterns_.end()) {
    return false;
  }
  return !it->second->MatchURL(url.GetWithEmptyPath()).empty();
}

}  // namespace performance_manager::execution_context_priority
