// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_handler.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/browser/ui/webui/theme_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tab_strip_resources.h"
#include "chrome/grit/tab_strip_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/resources/grit/webui_resources.h"

// These data types must be in all lowercase.
const char kWebUITabIdDataType[] = "application/vnd.chromium.tab";
const char kWebUITabGroupIdDataType[] = "application/vnd.chromium.tabgroup";

TabStripUI::TabStripUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::HostZoomMap::Get(web_ui->GetWebContents()->GetSiteInstance())
      ->SetZoomLevelForHostAndScheme(content::kChromeUIScheme,
                                     chrome::kChromeUITabStripHost, 0);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUITabStripHost);
  webui::SetupWebUIDataSource(
      html_source, base::make_span(kTabStripResources, kTabStripResourcesSize),
      IDR_TAB_STRIP_TAB_STRIP_HTML);

  html_source->AddString("tabIdDataType", kWebUITabIdDataType);
  html_source->AddString("tabGroupIdDataType", kWebUITabGroupIdDataType);

  // Add a load time string for the frame color to allow the tab strip to paint
  // a background color that matches the frame before any content loads
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile);
  html_source->AddString("frameColor",
                         color_utils::SkColorToRgbaString(
                             tp.GetColor(ThemeProperties::COLOR_FRAME_ACTIVE)));

  static constexpr webui::LocalizedString kStrings[] = {
      {"newTab", IDS_TOOLTIP_NEW_TAB},
      {"tabListTitle", IDS_ACCNAME_TAB_LIST},
      {"closeTab", IDS_ACCNAME_CLOSE},
      {"defaultTabTitle", IDS_DEFAULT_TAB_TITLE},
      {"loadingTab", IDS_TAB_LOADING_TITLE},
      {"tabCrashed", IDS_TAB_AX_LABEL_CRASHED_FORMAT},
      {"tabNetworkError", IDS_TAB_AX_LABEL_NETWORK_ERROR_FORMAT},
      {"audioPlaying", IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT},
      {"usbConnected", IDS_TAB_AX_LABEL_USB_CONNECTED_FORMAT},
      {"bluetoothConnected", IDS_TAB_AX_LABEL_BLUETOOTH_CONNECTED_FORMAT},
      {"hidConnected", IDS_TAB_AX_LABEL_HID_CONNECTED_FORMAT},
      {"serialConnected", IDS_TAB_AX_LABEL_SERIAL_CONNECTED_FORMAT},
      {"mediaRecording", IDS_TAB_AX_LABEL_MEDIA_RECORDING_FORMAT},
      {"audioMuting", IDS_TAB_AX_LABEL_AUDIO_MUTING_FORMAT},
      {"tabCapturing", IDS_TAB_AX_LABEL_DESKTOP_CAPTURING_FORMAT},
      {"pipPlaying", IDS_TAB_AX_LABEL_PIP_PLAYING_FORMAT},
      {"desktopCapturing", IDS_TAB_AX_LABEL_DESKTOP_CAPTURING_FORMAT},
      {"vrPresenting", IDS_TAB_AX_LABEL_VR_PRESENTING},
      {"unnamedGroupLabel", IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT},
      {"namedGroupLabel", IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT},
  };
  html_source->AddLocalizedStrings(kStrings);
  content::WebUIDataSource::Add(profile, html_source);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  web_ui->AddMessageHandler(std::make_unique<ThemeHandler>());
}

TabStripUI::~TabStripUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabStripUI)

void TabStripUI::Initialize(Browser* browser, TabStripUIEmbedder* embedder) {
  content::WebUI* const web_ui = TabStripUI::web_ui();
  DCHECK_EQ(Profile::FromWebUI(web_ui), browser->profile());
  auto handler = std::make_unique<TabStripUIHandler>(browser, embedder);
  handler_ = handler.get();
  web_ui->AddMessageHandler(std::move(handler));
}

void TabStripUI::LayoutChanged() {
  handler_->NotifyLayoutChanged();
}

void TabStripUI::ReceivedKeyboardFocus() {
  handler_->NotifyReceivedKeyboardFocus();
}
