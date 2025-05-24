// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/history/history_side_panel_ui.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/cr_components/history/history_util.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_history_resources.h"
#include "chrome/grit/side_panel_history_resources_map.h"
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
#include "ui/webui/webui_util.h"

HistorySidePanelUIConfig::HistorySidePanelUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIHistorySidePanelHost) {}

bool HistorySidePanelUIConfig::IsPreloadable() {
  return true;
}

bool HistorySidePanelUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(browser_context);
  // Keep in sync with history_clusters.mojom.PageHandler registration.
  // If the WebUI is enabled without registering the PageHandler, the WebUI will
  // crash on getting the PageHandler remote. history.mojom.PageHandler is
  // always registered.
  return history_clusters_service &&
         history_clusters_service->is_journeys_feature_flag_enabled();
}

std::optional<int> HistorySidePanelUIConfig::GetCommandIdForTesting() {
  return IDC_SHOW_HISTORY_SIDE_PANEL;
}

HistorySidePanelUI::HistorySidePanelUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIHistorySidePanelHost);

  HistoryUtil::PopulateSourceForSidePanelHistory(source, profile);

  HistoryClustersUtil::PopulateSource(source, profile, /*in_side_panel=*/true);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  source->AddBoolean(
      "enableHistoryEmbeddings",
      history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile) &&
          history_embeddings::GetFeatureParameters().enable_side_panel);
  history_embeddings::PopulateSourceForWebUI(source, profile);

  webui::SetupWebUIDataSource(source, kSidePanelHistoryResources,
                              IDR_SIDE_PANEL_HISTORY_HISTORY_HTML);

  source->AddResourcePaths(kSidePanelSharedResources);
}

HistorySidePanelUI::~HistorySidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistorySidePanelUI)

void HistorySidePanelUI::BindInterface(
    mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler) {
  browsing_history_handler_ = std::make_unique<BrowsingHistoryHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
  browsing_history_handler_->SetSidePanelUIEmbedder(this->embedder());
}

void HistorySidePanelUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void HistorySidePanelUI::BindInterface(
    mojo::PendingReceiver<history_clusters::mojom::PageHandler>
        pending_page_handler) {
  history_clusters_handler_ =
      std::make_unique<history_clusters::HistoryClustersHandler>(
          std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents(), browser_window_interface_);
  history_clusters_handler_->SetSidePanelUIEmbedder(this->embedder());
}

void HistorySidePanelUI::BindInterface(
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

void HistorySidePanelUI::BindInterface(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
        pending_page_handler) {
  history_embeddings_handler_ = std::make_unique<HistoryEmbeddingsHandler>(
      std::move(pending_page_handler),
      Profile::FromWebUI(web_ui())->GetWeakPtr(), web_ui(),
      /*for_side_panel=*/true);
}

base::WeakPtr<HistorySidePanelUI> HistorySidePanelUI::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HistorySidePanelUI::SetQuery(const std::string& query) {
  // If the handler has already been created, pass to the existing WebUI.
  // Otherwise, we don't need to do anything, because
  // HistoryClustersSidePanelCoordinator will pass it to the newly created WebUI
  // via a URL parameter.
  if (history_clusters_handler_) {
    history_clusters_handler_->SetQuery(query);
  }
}

std::string HistorySidePanelUI::GetLastQueryIssued() const {
  return history_clusters_handler_
             ? history_clusters_handler_->last_query_issued()
             : std::string();
}
