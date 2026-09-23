// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_service.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_backend.h"
#include "components/critical_actions/core/browser/features.h"

namespace critical_actions {

namespace {

std::string_view ActionSourceToString(ActionSource source) {
  switch (source) {
    case ActionSource::kPasswordManager:
      return "PasswordManager";
    case ActionSource::kActor:
      return "Actor";
    case ActionSource::kAutofill:
      return "Autofill";
    case ActionSource::kUnknown:
      return "Unknown";
  }
  NOTREACHED();
}

void LogVisitIdResolutionOutcome(ActionSource source,
                                 VisitIdResolutionOutcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.VisitIdResolutionOutcome.",
                    ActionSourceToString(source)}),
      outcome);
}

void LogConversationIdResolutionOutcome(
    ActionSource source,
    ConversationIdResolutionOutcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.ConversationIdResolutionOutcome.",
                    ActionSourceToString(source)}),
      outcome);
}

void LogConversationIdDeferredResolutionOutcome(
    ActionSource source,
    ConversationIdDeferredResolutionOutcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.ConversationIdDeferredResolutionOutcome.",
                    ActionSourceToString(source)}),
      outcome);
}

}  // namespace

CriticalActionService::CriticalActionService(
    const base::FilePath& db_path,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    history::HistoryService* history_service)
    : backend_(backend_task_runner, db_path),
      navigation_cache_(features::kMaxNavigationCacheCapacity.Get()),
      task_to_conversation_cache_(
          features::kMaxTaskToConversationCacheCapacity.Get()),
      task_to_critical_action_ids_cache_(
          features::kMaxTaskToCriticalActionIdsCacheCapacity.Get()) {
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
  for (auto& [nav_id, state] : navigation_cache_) {
    DropPendingActions(state,
                       VisitIdResolutionOutcome::kEvictedServiceShutdown);
  }
  navigation_cache_.Clear();
  task_to_conversation_cache_.Clear();

  for (const auto& [_, actions] : task_to_critical_action_ids_cache_) {
    for (const auto& pending_action : actions) {
      LogConversationIdDeferredResolutionOutcome(
          pending_action.action_source,
          ConversationIdDeferredResolutionOutcome::kEvictedServiceShutdown);
    }
  }
  task_to_critical_action_ids_cache_.Clear();
  backend_.Reset();
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
    LogVisitIdResolutionOutcome(entry.action_source,
                                VisitIdResolutionOutcome::kSuccess);
  }
  state.pending_actions.clear();
}

void CriticalActionService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void CriticalActionService::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  CriticalActionEntry resolved_entry = entry;
  ConversationIdResolutionOutcome conversation_id_outcome =
      MaybeSetConversationId(resolved_entry);
  LogConversationIdResolutionOutcome(resolved_entry.action_source,
                                     conversation_id_outcome);

  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.EventLogged.",
                    ActionSourceToString(resolved_entry.action_source)}),
      resolved_entry.action_type);
  backend_.AsyncCall(&CriticalActionBackend::AddCriticalAction)
      .WithArgs(resolved_entry);
}

void CriticalActionService::SetCriticalActionsConversationId(
    const std::vector<std::string>& actor_task_ids,
    std::string_view conversation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_ || actor_task_ids.empty() || conversation_id.empty()) {
    return;
  }

  // TODO(b/561944228): CriticalActionService needs conversation_id, this is a
  // temporary solution while b/494212836 is in place; remove once fixed.
  std::vector<std::string> critical_action_ids_to_update;
  for (const std::string& task_id : actor_task_ids) {
    // Always populate `task_to_conversation_cache_` so that any subsequent
    // critical actions logged for this task ID can immediately resolve their
    // conversation ID.
    task_to_conversation_cache_.Put(task_id, std::string(conversation_id));

    auto it = task_to_critical_action_ids_cache_.Get(task_id);
    if (it != task_to_critical_action_ids_cache_.end()) {
      for (const auto& pending_action : it->second) {
        LogConversationIdDeferredResolutionOutcome(
            pending_action.action_source,
            ConversationIdDeferredResolutionOutcome::kBackfilled);
        critical_action_ids_to_update.push_back(
            pending_action.critical_action_id);
      }
      task_to_critical_action_ids_cache_.Erase(it);
    }
  }

  if (!critical_action_ids_to_update.empty()) {
    backend_.AsyncCall(&CriticalActionBackend::SetCriticalActionsConversationId)
        .WithArgs(std::move(critical_action_ids_to_update),
                  std::string(conversation_id));
  }
}

void CriticalActionService::AddCriticalActionWithNavigationId(
    const CriticalActionEntry& entry,
    int64_t navigation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (navigation_id == 0) {
    LogVisitIdResolutionOutcome(
        entry.action_source, VisitIdResolutionOutcome::kDroppedNoNavigationId);
    return;
  }

  auto it = navigation_cache_.Get(navigation_id);
  if (it != navigation_cache_.end() && it->second.visit_id.has_value()) {
    CriticalActionEntry resolved_entry = entry;
    resolved_entry.visit_id = *it->second.visit_id;
    AddCriticalAction(resolved_entry);
    LogVisitIdResolutionOutcome(resolved_entry.action_source,
                                VisitIdResolutionOutcome::kSuccess);
    return;
  }

  if (it == navigation_cache_.end()) {
    if (navigation_cache_.size() >= navigation_cache_.max_size() &&
        !navigation_cache_.empty()) {
      DropPendingActions(navigation_cache_.rbegin()->second,
                         VisitIdResolutionOutcome::kEvictedCapacityExceeded);
    }
    it = navigation_cache_.Put(navigation_id, NavigationState());
  }
  it->second.pending_actions.push_back(entry);
}

void CriticalActionService::OnNavigationDiscarded(int64_t navigation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = navigation_cache_.Peek(navigation_id);
  if (it != navigation_cache_.end()) {
    DropPendingActions(it->second,
                       VisitIdResolutionOutcome::kEvictedNavigatedAway);
    navigation_cache_.Erase(it);
  }
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

void CriticalActionService::DropPendingActions(
    NavigationState& state,
    VisitIdResolutionOutcome outcome) {
  for (const auto& entry : state.pending_actions) {
    LogVisitIdResolutionOutcome(entry.action_source, outcome);
  }
  state.pending_actions.clear();
}

ConversationIdResolutionOutcome CriticalActionService::MaybeSetConversationId(
    CriticalActionEntry& entry) {
  if (!entry.conversation_id.empty()) {
    return ConversationIdResolutionOutcome::kAlreadyPresent;
  }
  if (entry.actor_task_id.empty()) {
    return ConversationIdResolutionOutcome::kMissingTaskId;
  }

  auto it = task_to_conversation_cache_.Get(entry.actor_task_id);
  if (it != task_to_conversation_cache_.end()) {
    entry.conversation_id = it->second;
    return ConversationIdResolutionOutcome::kResolvedFromCache;
  }

  auto critical_action_it =
      task_to_critical_action_ids_cache_.Get(entry.actor_task_id);
  if (critical_action_it != task_to_critical_action_ids_cache_.end()) {
    critical_action_it->second.push_back(
        PendingCriticalAction{entry.critical_action_id, entry.action_source});
    return ConversationIdResolutionOutcome::kDeferredCacheMiss;
  }

  if (task_to_critical_action_ids_cache_.size() >=
          task_to_critical_action_ids_cache_.max_size() &&
      task_to_critical_action_ids_cache_.max_size() > 0) {
    for (const auto& pending_action :
         task_to_critical_action_ids_cache_.rbegin()->second) {
      LogConversationIdDeferredResolutionOutcome(
          pending_action.action_source,
          ConversationIdDeferredResolutionOutcome::kEvictedCapacityExceeded);
    }
  }
  task_to_critical_action_ids_cache_.Put(
      entry.actor_task_id,
      std::vector<PendingCriticalAction>{
          {entry.critical_action_id, entry.action_source}});

  return ConversationIdResolutionOutcome::kDeferredCacheMiss;
}

}  // namespace critical_actions
