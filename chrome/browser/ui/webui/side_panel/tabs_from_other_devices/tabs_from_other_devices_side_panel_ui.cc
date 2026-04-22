// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_coordinator.h"
#include "chrome/browser/ui/webui/cr_components/history/history_util.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "chrome/grit/side_panel_tabs_from_other_devices_resources.h"
#include "chrome/grit/side_panel_tabs_from_other_devices_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/sessions/core/session_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_util.h"

TabsFromOtherDevicesUIConfig::TabsFromOtherDevicesUIConfig()
    : DefaultTopChromeWebUIConfig(
          content::kChromeUIScheme,
          chrome::kChromeUITabsFromOtherDevicesSidePanelHost) {}

bool TabsFromOtherDevicesUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return TabsFromOtherDevicesSidePanelCoordinator::IsSupported(
      Profile::FromBrowserContext(browser_context));
}

std::optional<int> TabsFromOtherDevicesUIConfig::GetCommandIdForTesting() {
  return IDC_SHOW_TABS_FROM_OTHER_DEVICES_SIDE_PANEL;
}

TabsFromOtherDevicesSidePanelUI::TabsFromOtherDevicesSidePanelUI(
    content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUITabsFromOtherDevicesSidePanelHost);

  HistoryUtil::PopulateCommonSourceForHistory(source, profile);

  static constexpr webui::LocalizedString kStrings[] = {
      {"noSyncedResults", IDS_HISTORY_NO_SYNCED_RESULTS},
      {"loading", IDS_HISTORY_LOADING},
      {"title", IDS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TITLE},
  };
  source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(
      source, kSidePanelTabsFromOtherDevicesResources,
      IDR_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_HTML);

  source->AddResourcePaths(kSidePanelSharedResources);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
}

TabsFromOtherDevicesSidePanelUI::~TabsFromOtherDevicesSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabsFromOtherDevicesSidePanelUI)

void TabsFromOtherDevicesSidePanelUI::SetBrowserWindowInterface(
    BrowserWindowInterface* browser_window_interface) {
  browser_window_interface_ = browser_window_interface;
}

void TabsFromOtherDevicesSidePanelUI::BindInterface(
    mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler>
        pending_page_handler) {
  foreign_session_handler_ =
      std::make_unique<browser_sync::ForeignSessionHandler>(
          std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents(),
          base::BindRepeating([](content::WebContents* source_web_contents,
                                 const ::sessions::SessionTab& tab,
                                 WindowOpenDisposition disposition) {
            SessionRestore::RestoreForeignSessionTab(source_web_contents, tab,
                                                     disposition);
          }),
          base::BindRepeating(
              [](Profile* profile,
                 const std::vector<const ::sessions::SessionWindow*>& windows) {
                SessionRestore::RestoreForeignSessionWindows(
                    profile, windows.begin(), windows.end(), base::DoNothing());
              }),
          this);
}
