// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_footer_resources.h"
#include "chrome/grit/new_tab_footer_resources_map.h"
#include "chrome/grit/new_tab_shared_resources.h"
#include "chrome/grit/new_tab_shared_resources_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_util.h"

NewTabFooterUIConfig::NewTabFooterUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUINewTabFooterHost) {}

bool NewTabFooterUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(ntp_features::kNtpFooter);
}

NewTabFooterUI::NewTabFooterUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, /*enable_chrome_send=*/true),
      webui_load_timer_(std::make_unique<WebuiLoadTimer>(
          web_ui->GetWebContents(),
          "NewTabPage.Footer.WebUI.LoadDocumentTime",
          "NewTabPage.Footer.WebUI.LoadCompletedTime")),
      customize_buttons_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)) {
  // Set up the chrome://newtab-footer source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUINewTabFooterHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kNewTabFooterResources,
                              IDR_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HTML);
  source->AddResourcePaths(kNewTabSharedResources);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"customizeButton", IDS_NTP_CUSTOMIZE_BUTTON_LABEL},
      {"customizeThisPage", IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL},
      {"customizeThisPageWallpaperSearch",
       IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_WALLPAPER_SEARCH_LABEL},
      {"manageExtension", IDS_MANAGE_EXTENSION},
      {"wallpaperSearchButton", IDS_NTP_WALLPAPER_SEARCH_PAGE_HEADER},
  };
  source->AddLocalizedStrings(kLocalizedStrings);
}

NewTabFooterUI::~NewTabFooterUI() = default;

// static
void NewTabFooterUI::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNtpFooterVisible, true);
}

void NewTabFooterUI::BindInterface(
    mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandlerFactory>
        pending_receiver) {
  if (document_factory_receiver_.is_bound()) {
    document_factory_receiver_.reset();
  }

  document_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabFooterUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void NewTabFooterUI::BindInterface(
    mojo::PendingReceiver<
        customize_buttons::mojom::CustomizeButtonsHandlerFactory>
        pending_receiver) {
  if (customize_buttons_factory_receiver_.is_bound()) {
    customize_buttons_factory_receiver_.reset();
  }
  customize_buttons_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabFooterUI::CreateNewTabFooterHandler(
    mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
        pending_document,
    mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
        pending_handler) {
  handler_ = std::make_unique<NewTabFooterHandler>(
      std::move(pending_handler), std::move(pending_document), this->embedder(),
      web_ui()->GetWebContents());
}

void NewTabFooterUI::CreateCustomizeButtonsHandler(
    mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
        pending_page,
    mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
        pending_page_handler) {
  customize_buttons_handler_ = std::make_unique<CustomizeButtonsHandler>(
      std::move(pending_page_handler), std::move(pending_page), web_ui(),
      /*tab_interface=*/nullptr,
      std::make_unique<NewTabPageFeaturePromoHelper>());
}

WEB_UI_CONTROLLER_TYPE_IMPL(NewTabFooterUI)
