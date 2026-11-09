// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_service.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_backend.h"
#include "components/critical_actions/core/browser/critical_action_metrics.h"
#include "components/critical_actions/core/browser/features.h"

namespace critical_actions {

namespace {

// Drops `entry.conversation_anchor_id` if it exceeds
// kMaxConversationAnchorIdLength. Returns true if it was dropped.
bool DropOversizedConversationAnchorId(CriticalActionEntry& entry) {
  if (entry.conversation_anchor_id.size() <= kMaxConversationAnchorIdLength) {
    return false;
  }
  DLOG(WARNING) << "Dropping oversized conversation_anchor_id (length="
                << entry.conversation_anchor_id.size() << ")";
  // Assign default-constructed std::string() rather than .clear() so any
  // oversized heap buffer is freed immediately before queuing in
  // `navigation_cache_`.
  entry.conversation_anchor_id = std::string();
  return true;
}

}  // namespace

CriticalActionService::CriticalActionService(
    const base::FilePath& db_path,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    history::HistoryService* history_service)
    : backend_(backend_task_runner, db_path),
      navigation_cache_(features::kMaxNavigationCacheCapacity.Get()),
      redirect_metrics_recorder_(
          features::kMaxVisitToActionsCacheCapacity.Get()),
      task_to_conversation_cache_(
          features::kMaxTaskToConversationCacheCapacity.Get()),
      task_to_critical_action_ids_cache_(
          features::kMaxTaskToCriticalActionIdsCacheCapacity.Get()),
      task_to_anchor_cache_(base::saturated_cast<size_t>(
          features::kMaxTaskToAnchorCacheCapacity.Get())) {
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
  weak_ptr_factory_.InvalidateWeakPtrs();
  history_service_observation_.Reset();
  for (auto& [nav_id, state] : navigation_cache_) {
    DropPendingActions(state,
                       VisitIdResolutionOutcome::kEvictedServiceShutdown);
  }
  navigation_cache_.Clear();
  redirect_metrics_recorder_.Clear();
  task_to_conversation_cache_.Clear();
  task_to_anchor_cache_.Clear();

  for (const auto& [_, actions] : task_to_critical_action_ids_cache_) {
    for (const auto& pending_action : actions) {
      RecordConversationIdDeferredResolutionOutcome(
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
  // Return early when clearing all history, as this wipes the entire database.
  if (deletion_info.IsAllHistory()) {
    DeleteCriticalActionsInTimeRange(base::Time(), base::Time::Max());
    return;
  }

  // Only delete by time range when restrict_urls() is unset (e.g., Clear
  // Browsing Data). Deleting individual items on chrome://history sets a
  // full-day time_range() restricted to specific URLs, which must be deleted
  // by deleted_visit_ids() below rather than wiping the entire day.
  if (deletion_info.time_range().IsValid() &&
      !deletion_info.restrict_urls().has_value()) {
    DeleteCriticalActionsInTimeRange(deletion_info.time_range().begin(),
                                     deletion_info.time_range().end());
  }

  // Always clean up by `deleted_visit_ids()` as well, since an action's
  // `timestamp` can fall outside `time_range()` even when its visit is inside.
  if (!deletion_info.deleted_visit_ids().empty()) {
    std::vector<int64_t> visit_ids(deletion_info.deleted_visit_ids().begin(),
                                   deletion_info.deleted_visit_ids().end());
    DeleteCriticalActionsByVisitIds(visit_ids);
  }
}

void CriticalActionService::OnURLVisitedWithNavigationId(
    history::HistoryService* history_service,
    const history::VisitedURLInfo& visited_url_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  redirect_metrics_recorder_.OnURLVisited(visited_url_info.visit_row);

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
    const ActionSource action_source = entry.action_source;
    const ActionType action_type = entry.action_type;
    entry.visit_id = visit_id;
    PersistCriticalAction(std::move(entry));
    RecordVisitIdResolutionOutcome(action_source,
                                   VisitIdResolutionOutcome::kSuccess);
    redirect_metrics_recorder_.OnActionResolvedToVisit(visit_id, action_source,
                                                       action_type);
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
  MaybeResolveConversationAnchorId(resolved_entry);
  PersistCriticalAction(std::move(resolved_entry));
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
        RecordConversationIdDeferredResolutionOutcome(
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

void CriticalActionService::SetActorTaskConversationAnchorId(
    std::string_view actor_task_id,
    std::string_view conversation_anchor_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Anchors are only consumed by action-level chat linkouts, so don't cache
  // (and later persist) them unless that feature is enabled.
  if (!backend_ || actor_task_id.empty() ||
      !features::IsActionLevelChatLinkoutsEnabled()) {
    return;
  }

  if (conversation_anchor_id.empty()) {
    auto it = task_to_anchor_cache_.Peek(actor_task_id);
    if (it != task_to_anchor_cache_.end()) {
      task_to_anchor_cache_.Erase(it);
    }
    return;
  }

  if (conversation_anchor_id.size() > kMaxConversationAnchorIdLength) {
    DLOG(WARNING) << "Dropping oversized conversation_anchor_id (length="
                  << conversation_anchor_id.size() << ")";
    MarkActorTaskConversationAnchorDroppedTooLong(actor_task_id);
    return;
  }

  task_to_anchor_cache_.Put(
      std::string(actor_task_id),
      CachedAnchor{.anchor_id = std::string(conversation_anchor_id)});
}

void CriticalActionService::MarkActorTaskConversationAnchorDroppedTooLong(
    std::string_view actor_task_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_ || actor_task_id.empty() ||
      !features::IsActionLevelChatLinkoutsEnabled()) {
    return;
  }
  task_to_anchor_cache_.Put(std::string(actor_task_id),
                            CachedAnchor{.was_dropped_too_long = true});
}

void CriticalActionService::AddCriticalActionWithNavigationId(
    const CriticalActionEntry& entry,
    int64_t navigation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  if (navigation_id == 0) {
    RecordVisitIdResolutionOutcome(
        entry.action_source, VisitIdResolutionOutcome::kDroppedNoNavigationId);
    return;
  }

  CriticalActionEntry resolved_entry = entry;
  MaybeResolveConversationAnchorId(resolved_entry);

  auto it = navigation_cache_.Get(navigation_id);
  if (it != navigation_cache_.end() && it->second.visit_id.has_value()) {
    const int64_t visit_id = *it->second.visit_id;
    const ActionSource action_source = resolved_entry.action_source;
    const ActionType action_type = resolved_entry.action_type;
    resolved_entry.visit_id = visit_id;
    PersistCriticalAction(std::move(resolved_entry));
    RecordVisitIdResolutionOutcome(action_source,
                                   VisitIdResolutionOutcome::kSuccess);
    redirect_metrics_recorder_.OnActionResolvedToVisit(visit_id, action_source,
                                                       action_type);
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
  it->second.pending_actions.push_back(std::move(resolved_entry));
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

base::WeakPtr<CriticalActionProvider> CriticalActionService::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
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
    RecordVisitIdResolutionOutcome(entry.action_source, outcome);
  }
  state.pending_actions.clear();
}

void CriticalActionService::MaybeResolveConversationAnchorId(
    CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!features::IsActionLevelChatLinkoutsEnabled()) {
    // `task_to_anchor_cache_` is empty in this case, so there is nothing to
    // resolve. Keep a caller-provided anchor as is (the database bounds it as
    // well), but drop an oversized one so queued entries stay bounded.
    DropOversizedConversationAnchorId(entry);
    return;
  }
  RecordConversationAnchorResolutionOutcome(entry.action_source,
                                            ResolveConversationAnchorId(entry));
}

ConversationAnchorResolutionOutcome
CriticalActionService::ResolveConversationAnchorId(CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!entry.conversation_anchor_id.empty()) {
    // An anchor set on the entry is the caller's explicit choice for this
    // action. If it is oversized, drop it rather than falling back to the
    // task's cached anchor, which may belong to a different turn.
    if (DropOversizedConversationAnchorId(entry)) {
      return ConversationAnchorResolutionOutcome::kDroppedTooLong;
    }
    return ConversationAnchorResolutionOutcome::kAlreadyPresent;
  }

  if (entry.actor_task_id.empty()) {
    return ConversationAnchorResolutionOutcome::kMissingTaskId;
  }

  auto it = task_to_anchor_cache_.Get(entry.actor_task_id);
  if (it == task_to_anchor_cache_.end()) {
    return ConversationAnchorResolutionOutcome::kCacheMiss;
  }
  if (it->second.was_dropped_too_long) {
    return ConversationAnchorResolutionOutcome::kDroppedTooLong;
  }

  entry.conversation_anchor_id = it->second.anchor_id;
  return ConversationAnchorResolutionOutcome::kResolvedFromCache;
}

void CriticalActionService::PersistCriticalAction(CriticalActionEntry entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }
  ConversationIdResolutionOutcome conversation_id_outcome =
      MaybeSetConversationId(entry);
  RecordConversationIdResolutionOutcome(entry.action_source,
                                        conversation_id_outcome);

  RecordEventLogged(entry.action_source, entry.action_type);
  backend_.AsyncCall(&CriticalActionBackend::AddCriticalAction)
      .WithArgs(std::move(entry));
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
      RecordConversationIdDeferredResolutionOutcome(
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
