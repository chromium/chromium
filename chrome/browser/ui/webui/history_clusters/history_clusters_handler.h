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
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace history_clusters {

// Handles bidirectional communication between the history clusters page and the
// browser.
class HistoryClustersHandler : public mojom::PageHandler,
                               public HistoryClustersService::Observer {
 public:
  HistoryClustersHandler(
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);
  HistoryClustersHandler(const HistoryClustersHandler&) = delete;
  HistoryClustersHandler& operator=(const HistoryClustersHandler&) = delete;
  ~HistoryClustersHandler() override;

  // mojom::PageHandler:
  void SetPage(mojo::PendingRemote<mojom::Page> pending_page) override;
  void ToggleVisibility(bool visible,
                        ToggleVisibilityCallback callback) override;
  void QueryClusters(mojom::QueryParamsPtr query_params) override;
  void RemoveVisits(std::vector<mojom::URLVisitPtr> visits,
                    RemoveVisitsCallback callback) override;

  // HistoryClustersService::Observer:
  void OnDebugMessage(const std::string& message) override;

 private:
  // Called with the result of querying clusters. Subsequently, `query_result`
  // is sent to the JS to update the UI. `query_start_time` is also passed to
  // allow for performance logging.
  void OnClustersQueryResult(base::TimeTicks query_start_time,
                             mojom::QueryResultPtr query_result);
  // Called with the set of removed visits. Subsequently, `visits` is sent to
  // the JS to update the UI.
  void OnVisitsRemoved(std::vector<mojom::URLVisitPtr> visits);

  Profile* profile_;
  content::WebContents* web_contents_;
  // Tracker for query requests to the HistoryClustersService.
  base::CancelableTaskTracker query_task_tracker_;
  // Tracker for remove requests to the HistoryClustersService.
  base::CancelableTaskTracker remove_task_tracker_;

  // Used to observe the service.
  base::ScopedObservation<HistoryClustersService,
                          HistoryClustersService::Observer>
      service_observation_{this};

  mojo::Remote<mojom::Page> page_;
  mojo::Receiver<mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<HistoryClustersHandler> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HANDLER_H_
