// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354691088): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"

#include "components/grit/history_clusters_internals_resources.h"
#include "components/grit/history_clusters_internals_resources_map.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_page_handler_impl.h"

HistoryClustersInternalsUI::HistoryClustersInternalsUI(
    content::WebUI* web_ui,
    history_clusters::HistoryClustersService* history_clusters_service,
    history::HistoryService* history_service,
    SetupWebUIDataSourceCallback set_up_data_source_callback)
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      history_clusters_service_(history_clusters_service),
      history_service_(history_service) {
  std::move(set_up_data_source_callback)
      .Run(base::make_span(kHistoryClustersInternalsResources),
           IDR_HISTORY_CLUSTERS_INTERNALS_HISTORY_CLUSTERS_INTERNALS_HTML);
}

HistoryClustersInternalsUI::~HistoryClustersInternalsUI() = default;

void HistoryClustersInternalsUI::BindInterface(
    mojo::PendingReceiver<history_clusters_internals::mojom::PageHandlerFactory>
        receiver) {
  // TODO(crbug.com/40215132): Remove the reset which is needed now since
  // |this| is reused on internals page reloads.
  history_clusters_internals_page_factory_receiver_.reset();
  history_clusters_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void HistoryClustersInternalsUI::CreatePageHandler(
    mojo::PendingRemote<history_clusters_internals::mojom::Page> page,
    mojo::PendingReceiver<history_clusters_internals::mojom::PageHandler>
        page_handler) {
  history_clusters_internals_page_handler_ =
      std::make_unique<HistoryClustersInternalsPageHandlerImpl>(
          std::move(page), std::move(page_handler), history_clusters_service_,
          history_service_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(HistoryClustersInternalsUI)
