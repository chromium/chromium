// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"

#include "components/grit/history_clusters_internals_resources.h"
#include "components/grit/history_clusters_internals_resources_map.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_page_handler_impl.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

HistoryClustersInternalsUI::HistoryClustersInternalsUI(
    content::WebUI* web_ui,
    history_clusters::HistoryClustersService* history_clusters_service,
    history::HistoryService* history_service)
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      history_clusters_service_(history_clusters_service),
      history_service_(history_service) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      history_clusters_internals::kChromeUIHistoryClustersInternalsHost);
  webui::SetupWebUIDataSource(
      source, kHistoryClustersInternalsResources,
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
