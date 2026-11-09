// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_metrics.h"
#include "components/critical_actions/core/browser/critical_action_provider.h"
#include "components/critical_actions/core/browser/critical_action_redirect_metrics_recorder.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace critical_actions {

class CriticalActionBackend;

// UI thread service for recording and retrieving critical action history.
class CriticalActionService : public KeyedService,
                              public CriticalActionProvider,
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

  // UI thread entry point to associate an actor task with its current opaque
  // conversation anchor ID (e.g., Glic turn ID). The anchor is stamped onto
  // critical actions for `actor_task_id` when they are logged, so callers must
  // set it before the actor performs the actions of that turn. Anchors set
  // later are not applied to actions that were already logged. Conversely, the
  // anchor is resolved when an action is logged, not when it was initiated, so
  // an action of an earlier turn that is logged after the next turn's anchor
  // was set (e.g. an asynchronous event) gets the newer anchor. Setting the
  // same anchor again (e.g. for each batch of actions of one turn) does not
  // change attribution; only a different anchor does.
  // Passing an empty `conversation_anchor_id` clears any cached anchor for
  // `actor_task_id`. Anchors longer than `kMaxConversationAnchorIdLength` are
  // treated as if MarkActorTaskConversationAnchorDroppedTooLong() had been
  // called. No-op unless features::IsActionLevelChatLinkoutsEnabled().
  // TODO(b/583743246): Capture the anchor when an action is initiated so that
  // asynchronously logged actions are attributed to the turn that caused them.
  virtual void SetActorTaskConversationAnchorId(
      std::string_view actor_task_id,
      std::string_view conversation_anchor_id);

  // UI thread entry point for callers that enforce their own, stricter anchor
  // length limit (e.g. Glic turn IDs). Invalidates any cached anchor for
  // `actor_task_id`. Until the next SetActorTaskConversationAnchorId() call,
  // actions for `actor_task_id` get no anchor and record
  // `ConversationAnchorResolutionOutcome::kDroppedTooLong` rather than
  // `kCacheMiss`. No-op unless features::IsActionLevelChatLinkoutsEnabled().
  virtual void MarkActorTaskConversationAnchorDroppedTooLong(
      std::string_view actor_task_id);

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

  // CriticalActionProvider:
  void GetCriticalActions(
      const CriticalActionQueryOptions& options,
      base::OnceCallback<void(std::vector<CriticalActionEntry>)> callback)
      override;
  base::WeakPtr<CriticalActionProvider> GetWeakPtr() override;

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

  struct CachedAnchor {
    // Non-empty and at most `kMaxConversationAnchorIdLength` bytes when
    // `was_dropped_too_long` is false; empty otherwise.
    std::string anchor_id;
    bool was_dropped_too_long = false;
  };

  void DropPendingActions(NavigationState& state,
                          VisitIdResolutionOutcome reason);

  // Resolves `entry.conversation_anchor_id` at enqueue time and records
  // CriticalActions.ConversationAnchorResolutionOutcome. When action-level
  // chat linkouts are disabled, only drops an oversized caller-provided anchor
  // and records nothing.
  void MaybeResolveConversationAnchorId(CriticalActionEntry& entry);

  // Resolves or validates `entry.conversation_anchor_id` at enqueue time.
  ConversationAnchorResolutionOutcome ResolveConversationAnchorId(
      CriticalActionEntry& entry);

  // Resolves `conversation_id`, records persistence metrics, and forwards
  // `entry` (whose `conversation_anchor_id` has already been resolved) to
  // `backend_`.
  void PersistCriticalAction(CriticalActionEntry entry);

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

  // Records CriticalActions.EventLoggedInRedirect.* from visits and resolved
  // actions reported by this service.
  CriticalActionRedirectMetricsRecorder redirect_metrics_recorder_;

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

  // Capacity-limited LRU cache mapping actor_task_id to its current
  // conversation anchor state. Only populated when action-level chat linkouts
  // are enabled. Cleared on Shutdown().
  base::LRUCache<std::string, CachedAnchor, std::less<>> task_to_anchor_cache_;

  base::WeakPtrFactory<CriticalActionService> weak_ptr_factory_{this};
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_
