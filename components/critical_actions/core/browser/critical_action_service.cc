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
#include "ui/base/page_transition_types.h"

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

bool IsRedirectChainStart(ui::PageTransition transition) {
  return (transition & ui::PAGE_TRANSITION_CHAIN_START) != 0;
}

bool IsRedirectChainContinuation(ui::PageTransition transition) {
  return ui::PageTransitionIsRedirect(transition) &&
         !IsRedirectChainStart(transition);
}

}  // namespace

CriticalActionService::CriticalActionService(
    const base::FilePath& db_path,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    history::HistoryService* history_service)
    : backend_(backend_task_runner, db_path),
      navigation_cache_(features::kMaxNavigationCacheCapacity.Get()),
      visits_to_actions_cache_(features::kMaxVisitToActionsCacheCapacity.Get()),
      visit_redirect_types_(features::kMaxVisitToActionsCacheCapacity.Get()),
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
  visits_to_actions_cache_.Clear();
  visit_redirect_types_.Clear();
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
  HandleVisitForRedirectMetric(visited_url_info.visit_row);

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
    HandleActionForRedirectMetric(entry, visit_id);
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
    HandleActionForRedirectMetric(resolved_entry, *it->second.visit_id);
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

void CriticalActionService::HandleVisitForRedirectMetric(
    const history::VisitRow& visit_row) {
  const int64_t referring_visit_id = visit_row.referring_visit;
  const ui::PageTransition transition = visit_row.transition;

  if (referring_visit_id == history::kInvalidVisitID ||
      !IsRedirectChainContinuation(transition)) {
    return;
  }

  // LINT.IfChange(RedirectTypeClassification)
  const RedirectType redirect_type =
      (transition & ui::PAGE_TRANSITION_SERVER_REDIRECT)
          ? RedirectType::kServer
          : RedirectType::kClient;
  // LINT.ThenChange(//components/critical_actions/core/browser/critical_action_service.h:RedirectType)

  // Remember the redirect so critical actions arriving for
  // `referring_visit_id` later are recorded immediately.
  visit_redirect_types_.Put(referring_visit_id, redirect_type);

  auto it = visits_to_actions_cache_.Peek(referring_visit_id);
  if (it == visits_to_actions_cache_.end()) {
    return;
  }
  for (const auto& action : it->second) {
    RecordEventLoggedInRedirectHistogram(redirect_type, action.action_source,
                                         action.action_type);
  }
  visits_to_actions_cache_.Erase(it);
}

void CriticalActionService::HandleActionForRedirectMetric(
    const CriticalActionEntry& entry,
    int64_t visit_id) {
  auto redirected_it = visit_redirect_types_.Peek(visit_id);
  if (redirected_it != visit_redirect_types_.end()) {
    RecordEventLoggedInRedirectHistogram(
        redirected_it->second, entry.action_source, entry.action_type);
    return;
  }

  // Defer recording for visits not yet known to be redirected, in case a
  // subsequent redirect hop from this visit is seen.
  auto it = visits_to_actions_cache_.Get(visit_id);
  if (it == visits_to_actions_cache_.end()) {
    it = visits_to_actions_cache_.Put(visit_id, std::vector<VisitAction>());
  }
  it->second.push_back(VisitAction{entry.action_source, entry.action_type});
}

// static
void CriticalActionService::RecordEventLoggedInRedirectHistogram(
    RedirectType redirect_type,
    ActionSource source,
    ActionType action_type) {
  // LINT.IfChange(RedirectTypeHistogramSuffix)
  const std::string_view redirect_type_suffix =
      redirect_type == RedirectType::kServer ? "ServerRedirect"
                                             : "ClientRedirect";
  // LINT.ThenChange(//components/critical_actions/core/browser/critical_action_service.h:RedirectType)
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.EventLoggedInRedirect.",
                    redirect_type_suffix, ".", ActionSourceToString(source)}),
      action_type);
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
