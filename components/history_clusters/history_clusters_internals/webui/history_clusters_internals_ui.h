// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_UI_H_
#define COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_UI_H_

#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/webui/resource_path.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace history {
class HistoryService;
}  // namespace history

namespace history_clusters {
class HistoryClustersService;
}  // namespace history_clusters

class HistoryClustersInternalsPageHandlerImpl;

// The WebUI controller for chrome://history-clusters-internals.
class HistoryClustersInternalsUI
    : public ui::MojoWebUIController,
      public history_clusters_internals::mojom::PageHandlerFactory {
 public:
  using SetupWebUIDataSourceCallback =
      base::OnceCallback<void(base::span<const webui::ResourcePath> resources,
                              int default_resource)>;

  explicit HistoryClustersInternalsUI(
      content::WebUI* web_ui,
      history_clusters::HistoryClustersService* history_clusters_service,
      history::HistoryService* history_service,
      SetupWebUIDataSourceCallback set_up_data_source_callback);
  ~HistoryClustersInternalsUI() override;

  HistoryClustersInternalsUI(const HistoryClustersInternalsUI&) = delete;
  HistoryClustersInternalsUI& operator=(const HistoryClustersInternalsUI&) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<
          history_clusters_internals::mojom::PageHandlerFactory> receiver);

 private:
  // history_clusters_internals::mojom::PageHandlerFactory impls.
  void CreatePageHandler(
      mojo::PendingRemote<history_clusters_internals::mojom::Page> page,
      mojo::PendingReceiver<history_clusters_internals::mojom::PageHandler>
          page_handler) override;

  // Not owned. Guaranteed to outlive |this|, since the history clusters keyed
  // service has the lifetime of Profile, while |this| has the lifetime of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<history_clusters::HistoryClustersService> history_clusters_service_;

  // Not owned. Guaranteed to outlive |this|, since the history keyed
  // service has the lifetime of Profile, while |this| has the lifetime of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<history::HistoryService> history_service_;

  std::unique_ptr<HistoryClustersInternalsPageHandlerImpl>
      history_clusters_internals_page_handler_;
  mojo::Receiver<history_clusters_internals::mojom::PageHandlerFactory>
      history_clusters_internals_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_WEBUI_HISTORY_CLUSTERS_INTERNALS_UI_H_
