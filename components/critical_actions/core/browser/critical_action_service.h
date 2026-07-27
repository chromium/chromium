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

  void DropPendingActions(NavigationState& state,
                          VisitIdResolutionOutcome reason);

  SEQUENCE_CHECKER(sequence_checker_);
  base::SequenceBound<CriticalActionBackend> backend_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Capacity-limited LRU cache for tracking recent navigation history
  // resolutions.
  base::LRUCache<int64_t, NavigationState> navigation_cache_;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_
