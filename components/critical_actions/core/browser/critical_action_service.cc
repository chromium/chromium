// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_service.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_backend.h"
#include "components/critical_actions/core/browser/features.h"

namespace critical_actions {

CriticalActionService::CriticalActionService(
    const base::FilePath& db_path,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    history::HistoryService* history_service)
    : backend_(backend_task_runner, db_path),
      navigation_cache_(features::kMaxNavigationCacheCapacity.Get()) {
  backend_.AsyncCall(&CriticalActionBackend::Init);
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

CriticalActionService::~CriticalActionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CriticalActionService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  history_service_observation_.Reset();
  // TODO(b/534705000): Register UMA metadata with 'EvictedServiceShutdown'.
  navigation_cache_.Clear();
  backend_.Reset();
}

void CriticalActionService::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::AddCriticalAction).WithArgs(entry);
}

void CriticalActionService::AddCriticalActionWithNavigationId(
    const CriticalActionEntry& entry,
    int64_t navigation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = navigation_cache_.Get(navigation_id);
  if (it != navigation_cache_.end() && it->second.visit_id.has_value()) {
    CriticalActionEntry resolved_entry = entry;
    resolved_entry.visit_id = *it->second.visit_id;
    AddCriticalAction(resolved_entry);
    return;
  }

  if (it == navigation_cache_.end()) {
    it = navigation_cache_.Put(navigation_id, NavigationState());
  }
  it->second.pending_actions.push_back(entry);
}

void CriticalActionService::OnURLVisitedWithNavigationId(
    history::HistoryService* history_service,
    const history::VisitedURLInfo& visited_url_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!visited_url_info.local_navigation_id.has_value()) {
    return;
  }
  int64_t nav_id = visited_url_info.local_navigation_id.value();
  int64_t visit_id = visited_url_info.visit_row.visit_id;

  auto it = navigation_cache_.Get(nav_id);
  if (it == navigation_cache_.end()) {
    it = navigation_cache_.Put(nav_id, NavigationState());
  }

  NavigationState& state = it->second;
  state.visit_id = visit_id;

  for (auto& entry : state.pending_actions) {
    entry.visit_id = visit_id;
    AddCriticalAction(entry);
  }
  state.pending_actions.clear();
}

void CriticalActionService::GetCriticalAction(
    std::string_view critical_action_id,
    base::OnceCallback<void(std::optional<CriticalActionEntry>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  if (!backend_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::GetCriticalAction)
      .WithArgs(std::string(critical_action_id))
      .Then(std::move(callback));
}

void CriticalActionService::GetCriticalActions(
    const CriticalActionQueryOptions& options,
    base::OnceCallback<void(std::vector<CriticalActionEntry>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  if (!backend_) {
    std::move(callback).Run({});
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::GetCriticalActions)
      .WithArgs(options)
      .Then(std::move(callback));
}

void CriticalActionService::DeleteCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::DeleteCriticalAction)
      .WithArgs(std::string(critical_action_id));
}

void CriticalActionService::DeleteCriticalActionsInTimeRange(
    base::Time start_time,
    base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::DeleteCriticalActionsInTimeRange)
      .WithArgs(start_time, end_time);
}

void CriticalActionService::DeleteCriticalActionsByVisitIds(
    const std::vector<int64_t>& visit_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  backend_.AsyncCall(&CriticalActionBackend::DeleteCriticalActionsByVisitIds)
      .WithArgs(visit_ids);
}

void CriticalActionService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (deletion_info.IsAllHistory()) {
    DeleteCriticalActionsInTimeRange(base::Time(), base::Time::Max());
  } else if (deletion_info.time_range().IsValid()) {
    DeleteCriticalActionsInTimeRange(deletion_info.time_range().begin(),
                                     deletion_info.time_range().end());
  } else if (!deletion_info.deleted_visit_ids().empty()) {
    std::vector<int64_t> visit_ids(deletion_info.deleted_visit_ids().begin(),
                                   deletion_info.deleted_visit_ids().end());
    DeleteCriticalActionsByVisitIds(visit_ids);
  }
}

void CriticalActionService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

}  // namespace critical_actions
