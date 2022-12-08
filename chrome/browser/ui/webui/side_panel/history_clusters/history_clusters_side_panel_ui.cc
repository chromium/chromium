// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/side_panel_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

HistoryClustersSidePanelUI::HistoryClustersSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui),
      content::WebContentsObserver(web_ui->GetWebContents()) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIHistoryClustersSidePanelHost);

  Profile* const profile = Profile::FromWebUI(web_ui);

  HistoryClustersUtil::PopulateSource(source, profile, /*in_side_panel=*/true);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  const webui::ResourcePath kHistoryClustersResources[] = {
      {"history_clusters/history_clusters.html",
       IDR_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HTML},
      {"history_clusters/app.js", IDR_SIDE_PANEL_HISTORY_CLUSTERS_APP_JS},
      {"history_clusters/app.html.js",
       IDR_SIDE_PANEL_HISTORY_CLUSTERS_APP_HTML_JS},
  };

  webui::SetupWebUIDataSource(
      source, kHistoryClustersResources,
      IDR_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

HistoryClustersSidePanelUI::~HistoryClustersSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistoryClustersSidePanelUI)

void HistoryClustersSidePanelUI::BindInterface(
    mojo::PendingReceiver<history_clusters::mojom::PageHandler>
        pending_page_handler) {
  history_clusters_handler_ =
      std::make_unique<history_clusters::HistoryClustersHandler>(
          std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents());
  history_clusters_handler_->SetSidePanelUIEmbedder(this->embedder());
}

base::WeakPtr<HistoryClustersSidePanelUI>
HistoryClustersSidePanelUI::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HistoryClustersSidePanelUI::SetQuery(const std::string& query) {
  // If the handler has already been created, pass to the existing WebUI.
  // Otherwise, we don't need to do anything, because
  // HistoryClustersSidePanelCoordinator will pass it to the newly created WebUI
  // via a URL parameter.
  if (history_clusters_handler_) {
    history_clusters_handler_->SetQuery(query);
  }
}

std::string HistoryClustersSidePanelUI::GetLastQueryIssued() const {
  return history_clusters_handler_
             ? history_clusters_handler_->last_query_issued()
             : std::string();
}

void HistoryClustersSidePanelUI::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_handle->GetURL().host_piece() !=
      chrome::kChromeUIHistoryClustersSidePanelHost) {
    return;
  }

  // Early exit in case we've already set the initial state once.
  auto* logger =
      history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
          navigation_handle->GetWebContents()->GetPrimaryPage());
  if (logger->initial_state()) {
    return;
  }

  logger->set_navigation_id(navigation_handle->GetNavigationId());
  logger->set_initial_state(metrics_initial_state_);
}
