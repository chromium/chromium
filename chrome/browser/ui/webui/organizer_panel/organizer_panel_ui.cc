// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_ui.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs_page_handler.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"
#include "chrome/browser/ui/webui/organizer_panel/tab_groups_organizer_page_handler.h"
#include "chrome/browser/ui/webui/tab_search/search_handler.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/organizer_panel_resources.h"
#include "chrome/grit/organizer_panel_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/webui/webui_util.h"

#if !BUILDFLAG(OPTIMIZE_WEBUI)
#include "chrome/grit/tab_group_shared_resources.h"
#include "chrome/grit/tab_group_shared_resources_map.h"
#endif  // !BUILDFLAG(OPTIMIZE_WEBUI)

OrganizerPanelUIConfig::OrganizerPanelUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIOrganizerPanelHost) {}

bool OrganizerPanelUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return organizer_panel::IsOrganizerPanelFeatureEnabled();
}

bool OrganizerPanelUIConfig::IsPreloadable() {
  return organizer_panel::IsOrganizerPanelFeatureEnabled();
}

std::optional<int> OrganizerPanelUIConfig::GetCommandIdForTesting() {
  return IDC_TAB_SEARCH;
}

std::optional<GURL> OrganizerPanelUIConfig::GetPreloadEngagementAliasURL()
    const {
  return GURL(chrome::kChromeUITabSearchURL);
}

OrganizerPanelUI::OrganizerPanelUI(content::WebUI* web_ui)
    // If the data source is shared, `enable_chrome_send` enables
    // `BindingsPolicyValue::kWebUi`, which is required for `SetWebUIProperty` /
    // `chrome.getVariableValue('profileLoadTimeData')`. No message is sent with
    // chrome.send().
    : TopChromeWebUIController(
          web_ui,
          /*enable_chrome_send=*/webui::IsWebUIDataSourceShared(web_ui),
          /*enable_chrome_histograms=*/true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIOrganizerPanelHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"close", IDS_CLOSE},
      {"closeTab", IDS_TAB_SEARCH_CLOSE_TAB},
      {"collapse", IDS_ORGANIZER_PANEL_COLLAPSE},
      {"createTabGroup", IDS_ORGANIZER_PANEL_CREATE_TAB_GROUP},
      {"currentlyPlaying", IDS_ORGANIZER_PANEL_CURRENTLY_PLAYING},
      {"expand", IDS_ORGANIZER_PANEL_EXPAND},
      {"noRecentlyClosedTabs", IDS_ORGANIZER_PANEL_NO_RECENTLY_CLOSED_TABS},
      {"noResults", IDS_ORGANIZER_PANEL_NO_RESULTS},
      {"oneTab", IDS_TAB_SEARCH_ONE_TAB},
      {"options", IDS_ORGANIZER_PANEL_OPTIONS},
      {"recentlyClosed", IDS_ORGANIZER_PANEL_RECENTLY_CLOSED},
      {"recentlyVisited", IDS_ORGANIZER_PANEL_RECENTLY_VISITED},
      {"searchTabs", IDS_TAB_SEARCH_SEARCH_TABS},
      {"showAll", IDS_ORGANIZER_PANEL_SHOW_ALL},
      {"showSome", IDS_ORGANIZER_PANEL_SHOW_SOME},
      {"signInOnPhone", IDS_SEND_TAB_TO_SELF_SIGN_IN_ON_PHONE},
      {"splitView", IDS_ORGANIZER_PANEL_SPLIT_VIEW},
      {"tabCount", IDS_TAB_SEARCH_TAB_COUNT},
      {"tabGroupMoreOptions", IDS_TAB_GROUP_MORE_OPTIONS},
      {"tabGroups", IDS_ORGANIZER_PANEL_TAB_GROUPS},
      {"tabsOnOtherDevices", IDS_ORGANIZER_PANEL_CROSS_DEVICE_TABS},
      {"title", IDS_ORGANIZER_PANEL},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("foreignTabsEnabled",
                     organizer_panel::IsOrganizerPanelForeignTabsEnabled());
  // The values that depend on the profile. If the data source is shared by the
  // windows of all profiles, WebUIRenderFrameCreated() passes them to each
  // frame instead.
  if (!webui::IsWebUIDataSourceShared(web_ui)) {
    source->AddLocalizedStrings(GetProfileLoadTimeData(
        webui::GetUserProfile(web_ui->GetWebContents())));
  }
  source->AddBoolean("diceSupportEnabled",
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                     true
#else
                     false
#endif
  );

  ui::Accelerator accelerator(ui::VKEY_A,
                              ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("shortcutText", accelerator.GetShortcutText());
  source->AddBoolean("useTabGroupColorRefresh",
                     features::IsTabGroupColorRefreshEnabled());

  webui::SetupWebUIDataSource(source, kOrganizerPanelResources,
                              IDR_ORGANIZER_PANEL_ORGANIZER_PANEL_HTML);
#if !BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePaths(kTabGroupSharedResources);
#endif

  {
    // The URL data sources are added to the profile of the WebUI, which is
    // shared by the windows of all profiles in TopChromeWebUIProfile.
    Profile* webui_profile = Profile::FromWebUI(web_ui);
    content::URLDataSource::Add(
        webui_profile, std::make_unique<FaviconSource>(
                           webui_profile, chrome::FaviconUrlFormat::kFavicon2));
    content::URLDataSource::Add(webui_profile,
                                std::make_unique<ThemeSource>(webui_profile));
  }
}

OrganizerPanelUI::~OrganizerPanelUI() = default;

// static
base::DictValue OrganizerPanelUI::GetProfileLoadTimeData(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return base::DictValue()
      .Set("isUserSignedIn",
           identity_manager && identity_manager->HasPrimaryAccount(
                                   signin::ConsentLevel::kSignin))
      .Set("isIncognitoMode", profile->IsIncognitoProfile());
}

void OrganizerPanelUI::WebUIRenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // If the data source isn't shared, the constructor added these values to it.
  if (!webui::IsWebUIDataSourceShared(web_ui())) {
    return;
  }
  if (std::optional<std::string> json = base::WriteJson(GetProfileLoadTimeData(
          webui::GetUserProfile(web_ui()->GetWebContents())))) {
    render_frame_host->SetWebUIProperty("profileLoadTimeData", *json);
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(OrganizerPanelUI)

void OrganizerPanelUI::BindInterface(
    mojo::PendingReceiver<organizer_panel::mojom::ForeignTabsPageHandlerFactory>
        receiver) {
  foreign_tabs_page_factory_receiver_.reset();
  foreign_tabs_page_factory_receiver_.Bind(std::move(receiver));
}

void OrganizerPanelUI::BindInterface(
    mojo::PendingReceiver<organizer_panel::mojom::PageHandlerFactory>
        receiver) {
  organizer_panel_page_factory_receiver_.reset();
  organizer_panel_page_factory_receiver_.Bind(std::move(receiver));
}

void OrganizerPanelUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void OrganizerPanelUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::SearchHandler> receiver) {
  search_handler_ = std::make_unique<SearchHandler>(std::move(receiver));
}

void OrganizerPanelUI::BindInterface(
    mojo::PendingReceiver<
        organizer_panel::mojom::TabGroupsOrganizerPageHandlerFactory>
        receiver) {
  tab_groups_page_factory_receiver_.reset();
  tab_groups_page_factory_receiver_.Bind(std::move(receiver));
}

void OrganizerPanelUI::CreatePageHandler(
    mojo::PendingReceiver<organizer_panel::mojom::ForeignTabsPageHandler>
        receiver) {
  foreign_tabs_page_handler_ = std::make_unique<ForeignTabsPageHandler>(
      std::move(receiver), web_ui()->GetWebContents());
}

void OrganizerPanelUI::CreatePageHandler(
    mojo::PendingRemote<organizer_panel::mojom::Page> page,
    mojo::PendingReceiver<organizer_panel::mojom::PageHandler> receiver) {
  if (!page.is_valid() || !receiver.is_valid()) {
    organizer_panel_page_factory_receiver_.ReportBadMessage(
        "Invalid page pending remote or receiver in CreatePageHandler");
    return;
  }
  organizer_panel_page_handler_ = std::make_unique<OrganizerPanelPageHandler>(
      std::move(receiver), std::move(page), web_ui()->GetWebContents());
}

void OrganizerPanelUI::CreatePageHandler(
    mojo::PendingRemote<organizer_panel::mojom::TabGroupsOrganizerPage> page,
    mojo::PendingReceiver<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
        receiver) {
  if (!page.is_valid() || !receiver.is_valid()) {
    tab_groups_page_factory_receiver_.ReportBadMessage(
        "Invalid page pending remote or receiver in CreatePageHandler");
    return;
  }
  tab_groups_organizer_page_handler_ =
      std::make_unique<TabGroupsOrganizerPageHandler>(
          std::move(receiver), std::move(page), web_ui()->GetWebContents());
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
      std::move(receiver), std::move(page), web_ui(), this, metrics_reporter,
      /*log_metrics=*/false);
}
