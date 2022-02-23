// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_PAGE_HANDLER_IMPL_H_
#define COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_PAGE_HANDLER_IMPL_H_

#include <string>

#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// Handler for the internals page to receive and forward the log messages.
class HistoryClustersInternalsPageHandlerImpl
    : public history_clusters::HistoryClustersService::Observer {
 public:
  HistoryClustersInternalsPageHandlerImpl(
      mojo::PendingRemote<history_clusters_internals::mojom::Page> page,
      history_clusters::HistoryClustersService* history_clusters_service);
  ~HistoryClustersInternalsPageHandlerImpl() override;

  HistoryClustersInternalsPageHandlerImpl(
      const HistoryClustersInternalsPageHandlerImpl&) = delete;
  HistoryClustersInternalsPageHandlerImpl& operator=(
      const HistoryClustersInternalsPageHandlerImpl&) = delete;

 private:
  // history_clusters::HistoryClustersService::Observer overrides.
  void OnDebugMessage(const std::string& message) override;

  mojo::Remote<history_clusters_internals::mojom::Page> page_;

  // Not owned. Guaranteed to outlive |this|, since the history clusters keyed
  // service has the lifetime of Profile, while |this| has the lifetime of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<history_clusters::HistoryClustersService> history_clusters_service_;
};

#endif  // COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_PAGE_HANDLER_IMPL_H_
