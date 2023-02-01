// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_search_companion_resources.h"
#include "chrome/grit/side_panel_search_companion_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

SearchCompanionSidePanelUI::SearchCompanionSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui, true), web_ui_(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUISearchCompanionSidePanelHost);

  Profile* const profile = Profile::FromWebUI(web_ui);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSidePanelSearchCompanionResources,
                      kSidePanelSearchCompanionResourcesSize),
      IDR_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
}

SearchCompanionSidePanelUI::~SearchCompanionSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(SearchCompanionSidePanelUI)

content::WebUI* SearchCompanionSidePanelUI::GetWebUi() {
  return web_ui_;
}

void SearchCompanionSidePanelUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::SearchCompanionPageHandlerFactory>
        receiver) {
  search_companion_page_factory_receiver_.reset();
  search_companion_page_factory_receiver_.Bind(std::move(receiver));
}

void SearchCompanionSidePanelUI::CreateSearchCompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::SearchCompanionPageHandler>
        receiver,
    mojo::PendingRemote<side_panel::mojom::SearchCompanionPage> page) {
  search_companion_page_handler_ = std::make_unique<SearchCompanionPageHandler>(
      std::move(receiver), std::move(page), this);
}
