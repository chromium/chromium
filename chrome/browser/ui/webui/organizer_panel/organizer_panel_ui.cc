// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_ui.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/organizer_panel_resources.h"
#include "chrome/grit/organizer_panel_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/webui/webui_util.h"

OrganizerPanelUIConfig::OrganizerPanelUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIOrganizerPanelHost) {}

OrganizerPanelUI::OrganizerPanelUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIOrganizerPanelHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"closeTab", IDS_TAB_SEARCH_CLOSE_TAB},
      {"openTabs", IDS_TAB_SEARCH_OPEN_TABS},
      {"oneTab", IDS_TAB_SEARCH_ONE_TAB},
      {"recentlyClosed", IDS_TAB_SEARCH_RECENTLY_CLOSED},
      {"searchTabs", IDS_TAB_SEARCH_SEARCH_TABS},
      {"tabCount", IDS_TAB_SEARCH_TAB_COUNT},
      {"tabGroups", IDS_ORGANIZER_PANEL_TAB_GROUPS},
      {"title", IDS_ORGANIZER_PANEL},
  };
  source->AddLocalizedStrings(kStrings);

  ui::Accelerator accelerator(ui::VKEY_A,
                              ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("shortcutText", accelerator.GetShortcutText());

  webui::SetupWebUIDataSource(source, kOrganizerPanelResources,
                              IDR_ORGANIZER_PANEL_ORGANIZER_PANEL_HTML);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

OrganizerPanelUI::~OrganizerPanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(OrganizerPanelUI)

void OrganizerPanelUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void OrganizerPanelUI::CreatePageHandler(
    mojo::PendingRemote<tab_search::mojom::Page> page,
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) {
  if (!page.is_valid() || !receiver.is_valid()) {
    page_factory_receiver_.ReportBadMessage(
        "Invalid page pending remote or receiver in CreatePageHandler");
    return;
  }
  MetricsReporterService* const service =
      MetricsReporterService::GetFromWebContents(web_ui()->GetWebContents());
  CHECK(service);
  MetricsReporter* const metrics_reporter = service->metrics_reporter();
  CHECK(metrics_reporter);
  page_handler_ = std::make_unique<TabSearchPageHandler>(
      std::move(receiver), std::move(page), web_ui(), this, metrics_reporter);
}
