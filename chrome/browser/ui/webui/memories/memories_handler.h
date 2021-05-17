// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/webui/memories/memories.mojom.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/memories.mojom.h"
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

// Handles bidirectional communication between memories page and the browser.
class MemoriesHandler
    : public history_clusters::mojom::PageHandler,
      public history_clusters::HistoryClustersService::Observer {
 public:
  MemoriesHandler(mojo::PendingReceiver<history_clusters::mojom::PageHandler>
                      pending_page_handler,
                  Profile* profile,
                  content::WebContents* web_contents);
  MemoriesHandler(const MemoriesHandler&) = delete;
  MemoriesHandler& operator=(const MemoriesHandler&) = delete;
  ~MemoriesHandler() override;

  // history_clusters::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<history_clusters::mojom::Page> pending_page) override;
  void QueryMemories(
      history_clusters::mojom::QueryParamsPtr query_params) override;
  void RemoveVisits(std::vector<history_clusters::mojom::VisitPtr> visits,
                    RemoveVisitsCallback callback) override;

  // history_clusters::HistoryClustersService::Observer:
  void OnMemoriesDebugMessage(const std::string& message) override;

 private:
  // Called with `memory_mojoms` and `continuation_query_params` when the
  // results of querying the HistoryClustersService are available. The latter is
  // created in anticipation of a continuation query. Subsequently, the bound
  // partially constructed `result_mojom` parameter is supplied with
  // `memory_mojoms` and `continuation_query_params` and sent to the JS.
  void OnMemoriesQueryResult(
      history_clusters::mojom::MemoriesResultPtr result_mojom,
      history_clusters::mojom::QueryParamsPtr continuation_query_params,
      std::vector<history_clusters::mojom::MemoryPtr> memory_mojoms);
  // Called with the set of removed visits. Subsequently, `visits` is sent to
  // the JS to update the UI.
  void OnVisitsRemoved(std::vector<history_clusters::mojom::VisitPtr> visits);

#if !defined(CHROME_BRANDED)
  using MemoriesQueryResultsCallback =
      base::OnceCallback<void(history_clusters::mojom::QueryParamsPtr,
                              std::vector<history_clusters::mojom::MemoryPtr>)>;
  void QueryHistoryService(
      history_clusters::mojom::QueryParamsPtr query_params,
      std::vector<history_clusters::mojom::MemoryPtr> memory_mojoms,
      MemoriesQueryResultsCallback callback);
  void OnHistoryQueryResults(
      history_clusters::mojom::QueryParamsPtr query_params,
      std::vector<history_clusters::mojom::MemoryPtr> memory_mojoms,
      MemoriesQueryResultsCallback callback,
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

  base::WeakPtrFactory<MemoriesHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
