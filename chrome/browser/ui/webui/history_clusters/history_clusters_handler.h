// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters.mojom.h"
#include "components/history_clusters/core/history_clusters.mojom.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

#if !defined(CHROME_BRANDED)
namespace history {
class QueryResults;
}  // namespace history
#endif

// Handles bidirectional communication between the history clusters page and the
// browser.
class HistoryClustersHandler
    : public history_clusters::mojom::PageHandler,
      public history_clusters::HistoryClustersService::Observer {
 public:
  HistoryClustersHandler(
      mojo::PendingReceiver<history_clusters::mojom::PageHandler>
          pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);
  HistoryClustersHandler(const HistoryClustersHandler&) = delete;
  HistoryClustersHandler& operator=(const HistoryClustersHandler&) = delete;
  ~HistoryClustersHandler() override;

  // history_clusters::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<history_clusters::mojom::Page> pending_page) override;
  void QueryClusters(
      history_clusters::mojom::QueryParamsPtr query_params) override;
  void RemoveVisits(std::vector<history_clusters::mojom::URLVisitPtr> visits,
                    RemoveVisitsCallback callback) override;

  // history_clusters::HistoryClustersService::Observer:
  void OnMemoriesDebugMessage(const std::string& message) override;

 private:
  // Called with the original `query_params`, `continuation_max_time` which is
  // created in anticipation of the next query, and `cluster_mojoms` when the
  // results of querying the HistoryClustersService are available. Subsequently
  // creates a QueryResult instance using the parameters and sends it to the JS.
  void OnClustersQueryResult(
      history_clusters::mojom::QueryParamsPtr original_query_params,
      const absl::optional<base::Time>& continuation_max_time,
      std::vector<history_clusters::mojom::ClusterPtr> cluster_mojoms);
  // Called with the set of removed visits. Subsequently, `visits` is sent to
  // the JS to update the UI.
  void OnVisitsRemoved(
      std::vector<history_clusters::mojom::URLVisitPtr> visits);

#if !defined(CHROME_BRANDED)
  using QueryResultsCallback = base::OnceCallback<void(
      const absl::optional<base::Time>&,
      std::vector<history_clusters::mojom::ClusterPtr>)>;
  void QueryHistoryService(
      const std::string& query,
      base::Time max_time,
      size_t max_count,
      std::vector<history_clusters::mojom::ClusterPtr> cluster_mojoms,
      QueryResultsCallback callback);
  void OnHistoryQueryResults(
      const std::string& query,
      base::Time max_time,
      size_t max_count,
      std::vector<history_clusters::mojom::ClusterPtr> cluster_mojoms,
      QueryResultsCallback callback,
      history::QueryResults results);
#endif

  Profile* profile_;
  content::WebContents* web_contents_;
  // Tracker for query requests to the HistoryClustersService.
  base::CancelableTaskTracker query_task_tracker_;
  // Tracker for remove requests to the HistoryClustersService.
  base::CancelableTaskTracker remove_task_tracker_;

  // Used to observe the service.
  base::ScopedObservation<history_clusters::HistoryClustersService,
                          history_clusters::HistoryClustersService::Observer>
      service_observation_{this};

  mojo::Remote<history_clusters::mojom::Page> page_;
  mojo::Receiver<history_clusters::mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<HistoryClustersHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HANDLER_H_
