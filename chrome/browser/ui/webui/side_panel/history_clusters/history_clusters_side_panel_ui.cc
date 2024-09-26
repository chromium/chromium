// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_history_clusters_resources.h"
#include "chrome/grit/side_panel_history_clusters_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history_clusters/core/features.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_handler.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

HistoryClustersSidePanelUIConfig::HistoryClustersSidePanelUIConfig()
    : DefaultTopChromeWebUIConfig(
          content::kChromeUIScheme,
          chrome::kChromeUIHistoryClustersSidePanelHost) {}

bool HistoryClustersSidePanelUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(history_clusters::kSidePanelJourneys);
}

bool HistoryClustersSidePanelUIConfig::IsPreloadable() {
  return true;
}

std::optional<int> HistoryClustersSidePanelUIConfig::GetCommandIdForTesting() {
  return IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL;
}

HistoryClustersSidePanelUI::HistoryClustersSidePanelUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui),
      content::WebContentsObserver(web_ui->GetWebContents()) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIHistoryClustersSidePanelHost);

  HistoryClustersUtil::PopulateSource(source, profile, /*in_side_panel=*/true);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  source->AddBoolean(
      "enableHistoryEmbeddings",
      history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile) &&
          history_embeddings::kEnableSidePanel.Get());
  history_embeddings::PopulateSourceForWebUI(source);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSidePanelHistoryClustersResources,
                      kSidePanelHistoryClustersResourcesSize),
      IDR_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));
}

HistoryClustersSidePanelUI::~HistoryClustersSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistoryClustersSidePanelUI)

void HistoryClustersSidePanelUI::SetBrowserWindowInterface(
    BrowserWindowInterface* browser_window_interface) {
  browser_window_interface_ = browser_window_interface;
}

void HistoryClustersSidePanelUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void HistoryClustersSidePanelUI::BindInterface(
    mojo::PendingReceiver<history_clusters::mojom::PageHandler>
        pending_page_handler) {
  history_clusters_handler_ =
      std::make_unique<history_clusters::HistoryClustersHandler>(
          std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents(), browser_window_interface_);
  history_clusters_handler_->SetSidePanelUIEmbedder(this->embedder());
}

void HistoryClustersSidePanelUI::BindInterface(
    mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
        pending_page_handler) {
  base::WeakPtr<page_image_service::ImageService> image_service_weak;
  if (auto* image_service =
          page_image_service::ImageServiceFactory::GetForBrowserContext(
              Profile::FromWebUI(web_ui()))) {
    image_service_weak = image_service->GetWeakPtr();
  }
  image_service_handler_ =
      std::make_unique<page_image_service::ImageServiceHandler>(
          std::move(pending_page_handler), std::move(image_service_weak));
}

void HistoryClustersSidePanelUI::BindInterface(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
        pending_page_handler) {
  history_embeddings_handler_ = std::make_unique<HistoryEmbeddingsHandler>(
      std::move(pending_page_handler),
      Profile::FromWebUI(web_ui())->GetWeakPtr(), web_ui());
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

void HistoryClustersSidePanelUI::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    return;
  }
  history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
      web_ui()->GetWebContents()->GetPrimaryPage())
      ->WasShown();
}
