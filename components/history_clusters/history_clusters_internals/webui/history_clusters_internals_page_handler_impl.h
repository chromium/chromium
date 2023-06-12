// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_PAGE_HANDLER_IMPL_H_
#define COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_PAGE_HANDLER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Handler for the internals page to receive and forward the log messages.
class HistoryClustersInternalsPageHandlerImpl
    : public history_clusters_internals::mojom::PageHandler,
      public history_clusters::HistoryClustersService::Observer {
 public:
  HistoryClustersInternalsPageHandlerImpl(
      mojo::PendingRemote<history_clusters_internals::mojom::Page> page,
      mojo::PendingReceiver<history_clusters_internals::mojom::PageHandler>
          pending_page_handler,
      history_clusters::HistoryClustersService* history_clusters_service,
      history::HistoryService* history_service);
  ~HistoryClustersInternalsPageHandlerImpl() override;

  HistoryClustersInternalsPageHandlerImpl(
      const HistoryClustersInternalsPageHandlerImpl&) = delete;
  HistoryClustersInternalsPageHandlerImpl& operator=(
      const HistoryClustersInternalsPageHandlerImpl&) = delete;

 private:
  // history_clusters::mojom::PageHandler:
  void GetContextClustersJson(GetContextClustersJsonCallback callback) override;
  void PrintKeywordBagStateToLogMessages() override;

  // Gets most recent context clusters from HistoryService.
  void GetContextClusters(
      history_clusters::QueryClustersContinuationParams continuation_params,
      std::vector<history::Cluster> previously_retrieved_clusters,
      GetContextClustersJsonCallback callback);

  // Callback invoked when HistoryService returns context clusters.
  void OnGotContextClusters(
      std::vector<history::Cluster> previously_retrieved_clusters,
      GetContextClustersJsonCallback callback,
      std::vector<history::Cluster> new_clusters,
      history_clusters::QueryClustersContinuationParams continuation_params);

  // history_clusters::HistoryClustersService::Observer:
  void OnDebugMessage(const std::string& message) override;

  mojo::Remote<history_clusters_internals::mojom::Page> page_;
  mojo::Receiver<history_clusters_internals::mojom::PageHandler> page_handler_;

  base::CancelableTaskTracker task_tracker_;

  // Used to hold the task while we query for the context clusters.
  std::unique_ptr<history_clusters::HistoryClustersServiceTask>
      query_context_clusters_task_;

  // Not owned. Guaranteed to outlive |this|, since the history clusters keyed
  // service has the lifetime of Profile, while |this| has the lifetime of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<history_clusters::HistoryClustersService> history_clusters_service_;

  // Not owned. Guaranteed to outlive |this|, since the history keyed
  // service has the lifetime of Profile, while |this| has the lifetime of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<history::HistoryService> history_service_;

  // Used for async callbacks.
  base::WeakPtrFactory<HistoryClustersInternalsPageHandlerImpl>
      weak_ptr_factory_{this};
};

#endif  // COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_PAGE_HANDLER_IMPL_H_
