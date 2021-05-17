// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/memories.mojom.h"
#include "components/history_clusters/core/memories_remote_model_helper.h"
#include "components/history_clusters/core/visit_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace history_clusters {

// This Service is the API for UIs to fetch Chrome Memories.
class HistoryClustersService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMemoriesDebugMessage(const std::string& message) = 0;
  };

  struct QueryMemoriesResponse {
    QueryMemoriesResponse(mojom::QueryParamsPtr query_params,
                          std::vector<mojom::MemoryPtr> clusters);
    QueryMemoriesResponse(QueryMemoriesResponse&& other);
    ~QueryMemoriesResponse();
    mojom::QueryParamsPtr query_params;
    std::vector<mojom::MemoryPtr> clusters;
  };

  explicit HistoryClustersService(
      history::HistoryService* history_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  HistoryClustersService(const HistoryClustersService&) = delete;
  HistoryClustersService& operator=(const HistoryClustersService&) = delete;
  ~HistoryClustersService() override;

  // KeyedService:
  void Shutdown() override;

  // Used to add and remove observers.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Notifies the observers of a debug message being available.
  void NotifyDebugMessage(const std::string& message) const;

  // TODO(manukh) `HistoryClustersService` should be responsible for
  // constructing the
  //  `AnnotatedVisit`s rather than exposing these methods which are used by
  //  `HistoryClustersTabHelper` to construct the visits.
  // Gets an `IncompleteVisitContextAnnotations` after DCHECKing it exists; this
  // saves the call sites the effort.
  IncompleteVisitContextAnnotations& GetIncompleteVisitContextAnnotations(
      int64_t nav_id);
  // Gets or creates an `IncompleteVisitContextAnnotations`.
  IncompleteVisitContextAnnotations&
  GetOrCreateIncompleteVisitContextAnnotations(int64_t nav_id);
  // Returns whether an `IncompleteVisitContextAnnotations` exists.
  // TODO(manukh): Merge `HasIncompleteVisitContextAnnotations()` and
  //  `GetIncompleteVisitContextAnnotations()`.
  bool HasIncompleteVisitContextAnnotations(int64_t nav_id);
  // Completes the `IncompleteVisitContextAnnotations` if the expected metrics
  // have been recorded. References retrieved prior will no longer be valid.
  void CompleteVisitContextAnnotationsIfReady(int64_t nav_id);

  // Returns the freshest Memories created from the user visit history, in
  // reverse chronological order, based on the parameters in `query_params`
  // along with continuation query params meant to be used in the follow-up
  // request to load older Memories.
  // Note: At the moment, this method asks `remote_model_helper_` to construct
  // Memories from `visits_`.
  void QueryMemories(mojom::QueryParamsPtr query_params,
                     base::OnceCallback<void(QueryMemoriesResponse)> callback,
                     base::CancelableTaskTracker* task_tracker);
  // Removes all visits to the specified URLs in the specified time ranges in
  // `expire_list`. Calls `closure` when done.
  void RemoveVisits(const std::vector<history::ExpireHistoryArgs>& expire_list,
                    base::OnceClosure closure,
                    base::CancelableTaskTracker* task_tracker);

 private:
  friend class HistoryClustersServiceTestApi;

  // If the Memories flag is enabled, this contains all the visits in-memory
  // during the Profile lifetime. If the `kPersistContextAnnotationsInHistoryDb`
  // param is true, this will be empty.
  // TODO(tommycli): Hide this better behind a new debug flag.
  std::vector<history::AnnotatedVisit> visits_;

  // `VisitContextAnnotations`s are constructed stepwise; they're initially
  // placed in `incomplete_visit_context_annotations_` and moved either to
  // `visits_` or the history database once completed.
  std::map<int64_t, IncompleteVisitContextAnnotations>
      incomplete_visit_context_annotations_;

  history::HistoryService* history_service_;

  // Helper service to handle communicating with the remote model. This will be
  // used for debugging only; the launch ready feature will use a local model
  // instead.
  std::unique_ptr<MemoriesRemoteModelHelper> remote_model_helper_;

  // A list of observers for this service.
  base::ObserverList<Observer> observers_;

  // Used to asyncly call into `remote_model_helper_` after async history
  // request.
  std::unique_ptr<base::WeakPtrFactory<MemoriesRemoteModelHelper>>
      remote_model_helper_weak_factory_;
  // Weak pointers issued from this factory never get invalidated before the
  // service is destroyed.
  base::WeakPtrFactory<HistoryClustersService> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
