// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace critical_actions {

// Outcome of resolving a conversation ID for critical action entries.
// LINT.IfChange(ConversationIdResolutionOutcome)
enum class ConversationIdResolutionOutcome {
  kAlreadyPresent = 0,
  kResolvedFromCache = 1,
  kMissingTaskId = 2,
  kDeferredCacheMiss = 3,
  kMaxValue = kDeferredCacheMiss,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionConversationIdResolutionOutcome)

// Outcome of deferred conversation ID resolution for critical action entries.
// LINT.IfChange(ConversationIdDeferredResolutionOutcome)
enum class ConversationIdDeferredResolutionOutcome {
  kBackfilled = 0,
  kEvictedServiceShutdown = 1,
  kEvictedCapacityExceeded = 2,
  kMaxValue = kEvictedCapacityExceeded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionConversationIdDeferredResolutionOutcome)

// Outcome of resolving a navigation ID to a History Visit ID for critical
// action entries.
// LINT.IfChange(VisitIdResolutionOutcome)
enum class VisitIdResolutionOutcome {
  kSuccess = 0,
  kEvictedCapacityExceeded = 1,
  kEvictedNavigatedAway = 2,
  kDroppedNoNavigationId = 3,
  kEvictedServiceShutdown = 4,
  kMaxValue = kEvictedServiceShutdown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionVisitIdResolutionOutcome)

class CriticalActionBackend;

// UI thread service for recording and retrieving critical action history.
class CriticalActionService : public KeyedService,
                              public history::HistoryServiceObserver {
 public:
  CriticalActionService(
      const base::FilePath& db_path,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      history::HistoryService* history_service = nullptr);
  CriticalActionService(const CriticalActionService&) = delete;
  CriticalActionService& operator=(const CriticalActionService&) = delete;
  ~CriticalActionService() override;

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  void OnURLVisitedWithNavigationId(
      history::HistoryService* history_service,
      const history::VisitedURLInfo& visited_url_info) override;

  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // UI thread entry point to add a new critical action.
  virtual void AddCriticalAction(const CriticalActionEntry& entry);

  // UI thread entry point to associate conversation_id with tasks.
  virtual void SetCriticalActionsConversationId(
      const std::vector<std::string>& actor_task_ids,
      std::string_view conversation_id);

  // UI thread entry point to log or queue a critical action linked by
  // navigation ID.
  virtual void AddCriticalActionWithNavigationId(
      const CriticalActionEntry& entry,
      int64_t navigation_id);

  // UI thread entry point to notify that a navigation was discarded or
  // navigated away before Visit ID resolution completed.
  virtual void OnNavigationDiscarded(int64_t navigation_id);

  // UI thread entry point to retrieve a critical action record by ID.
  void GetCriticalAction(
      std::string_view critical_action_id,
      base::OnceCallback<void(std::optional<CriticalActionEntry>)> callback);

  // UI thread entry point to retrieve critical action records matching
  // `options`.
  void GetCriticalActions(
      const CriticalActionQueryOptions& options,
      base::OnceCallback<void(std::vector<CriticalActionEntry>)> callback);

  // UI thread entry point to delete a critical action record by ID.
  void DeleteCriticalAction(std::string_view critical_action_id);

  // UI thread entry point to delete all critical action records in given range.
  void DeleteCriticalActionsInTimeRange(base::Time start_time,
                                        base::Time end_time);

  // UI thread entry point to delete all critical action records associated with
  // the given visit IDs.
  void DeleteCriticalActionsByVisitIds(const std::vector<int64_t>& visit_ids);

 private:
  struct NavigationState {
    std::optional<int64_t> visit_id;
    std::vector<CriticalActionEntry> pending_actions;
  };

  // Type of redirect that took a visit to another visit.
  //
  // New values (e.g. "internal" redirects) must also be handled in the
  // classification and histogram code linked below.
  // LINT.IfChange(RedirectType)
  enum class RedirectType {
    kServer,
    kClient,
  };
  // LINT.ThenChange(//components/critical_actions/core/browser/critical_action_service.cc:RedirectTypeClassification, //components/critical_actions/core/browser/critical_action_service.cc:RedirectTypeHistogramSuffix, //tools/metrics/histograms/metadata/critical_actions/histograms.xml:CriticalActionRedirectType)

  // A critical action logged on a visit, cached in `visits_to_actions_cache_`.
  struct VisitAction {
    ActionSource action_source;
    ActionType action_type;
  };

  void DropPendingActions(NavigationState& state,
                          VisitIdResolutionOutcome reason);

  // Called for every new visit. If it continues a redirect chain, marks its
  // referring visit as redirected and records the histogram for any actions
  // waiting on the referring visit.
  void HandleVisitForRedirectMetric(const history::VisitRow& visit_row);

  // Called for every critical action resolved to `visit_id`. Records the
  // histogram if the visit is already known to be redirected, otherwise keeps
  // the action until a redirect away from the visit is seen.
  void HandleActionForRedirectMetric(const CriticalActionEntry& entry,
                                     int64_t visit_id);

  // Records CriticalActions.EventLoggedInRedirect.{RedirectType}.{ActionSource}
  // for a critical action of `action_type` from `source` whose visit was
  // redirected away by `redirect_type`.
  static void RecordEventLoggedInRedirectHistogram(RedirectType redirect_type,
                                                   ActionSource source,
                                                   ActionType action_type);

  // TODO(b/561944228): Temporary solution while b/494212836 is in place; remove
  // once fixed. CriticalActionService needs conversation_id.
  ConversationIdResolutionOutcome MaybeSetConversationId(
      CriticalActionEntry& entry);

  SEQUENCE_CHECKER(sequence_checker_);
  base::SequenceBound<CriticalActionBackend> backend_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Capacity-limited LRU cache for tracking recent navigation history
  // resolutions.
  base::LRUCache<int64_t, NavigationState> navigation_cache_;

  // Capacity-limited LRU cache of visits that have critical actions but are
  // not yet known to have been redirected. Maps visit_id to the actions
  // logged on it whose EventLoggedInRedirect recording is deferred.
  base::LRUCache<int64_t, std::vector<VisitAction>> visits_to_actions_cache_;

  // Capacity-limited LRU cache of visits known to have been redirected to
  // another visit, regardless of whether they have critical actions yet. Maps
  // visit_id to the redirect type (server or client).
  base::LRUCache<int64_t, RedirectType> visit_redirect_types_;

  // Capacity-limited LRU cache mapping actor_task_id to conversation_id. Unlike
  // `task_to_critical_action_ids_cache_`, this cache does not hold pending
  // action records, evicting entries here causes no data loss, so eviction
  // metrics are not tracked.
  // TODO(b/561944228): Remove when b/494212836 is fixed and conversation_id is
  // available when logging critical actions.
  base::LRUCache<std::string, std::string> task_to_conversation_cache_;

  struct PendingCriticalAction {
    std::string critical_action_id;
    ActionSource action_source;
  };

  // Capacity-limited LRU cache mapping actor_task_id to pending critical
  // actions.
  // TODO(b/561944228): Remove when b/494212836 is fixed and conversation_id is
  // available when logging critical actions.
  base::LRUCache<std::string, std::vector<PendingCriticalAction>>
      task_to_critical_action_ids_cache_;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_
