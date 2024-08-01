// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_page_handler.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
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
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

TabStripUI::TabStripUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /* enable_chrome_send */ true),
      webui_load_timer_(web_ui->GetWebContents(),
                        "WebUITabStrip.LoadDocumentTime",
                        "WebUITabStrip.LoadCompletedTime") {
  content::HostZoomMap::Get(web_ui->GetWebContents()->GetSiteInstance())
      ->SetZoomLevelForHostAndScheme(content::kChromeUIScheme,
                                     chrome::kChromeUITabStripHost, 0);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(profile,
                                             chrome::kChromeUITabStripHost);
  webui::SetupWebUIDataSource(
      html_source, base::make_span(kTabStripResources, kTabStripResourcesSize),
      IDR_TAB_STRIP_TAB_STRIP_HTML);

  html_source->AddString("tabIdDataType", kWebUITabIdDataType);
  html_source->AddString("tabGroupIdDataType", kWebUITabGroupIdDataType);

  static constexpr webui::LocalizedString kStrings[] = {
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
      {"audioRecording", IDS_TAB_AX_LABEL_AUDIO_RECORDING_FORMAT},
      {"videoRecording", IDS_TAB_AX_LABEL_VIDEO_RECORDING_FORMAT},
      {"audioMuting", IDS_TAB_AX_LABEL_AUDIO_MUTING_FORMAT},
      {"tabCapturing", IDS_TAB_AX_LABEL_DESKTOP_CAPTURING_FORMAT},
      {"pipPlaying", IDS_TAB_AX_LABEL_PIP_PLAYING_FORMAT},
      {"desktopCapturing", IDS_TAB_AX_LABEL_DESKTOP_CAPTURING_FORMAT},
      {"vrPresenting", IDS_TAB_AX_LABEL_VR_PRESENTING},
      {"unnamedGroupLabel", IDS_GROUP_AX_LABEL_UNNAMED_GROUP_FORMAT},
      {"namedGroupLabel", IDS_GROUP_AX_LABEL_NAMED_GROUP_FORMAT},
  };
  html_source->AddLocalizedStrings(kStrings);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
}

TabStripUI::~TabStripUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabStripUI)

void TabStripUI::BindInterface(
    mojo::PendingReceiver<tab_strip::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TabStripUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void TabStripUI::CreatePageHandler(
    mojo::PendingRemote<tab_strip::mojom::Page> page,
    mojo::PendingReceiver<tab_strip::mojom::PageHandler> receiver) {
  // Initialize() must be called immediately after LoadURL() for the WebUI
  // Tab Strip to start correctly. Only create TabStripPageHandler when both
  // browser_ and embedder_ are set after calling Initialize().
  if (browser_ && embedder_) {
    page_handler_ = std::make_unique<TabStripPageHandler>(
        std::move(receiver), std::move(page), web_ui(), browser_, embedder_);
  }
}

void TabStripUI::Initialize(Browser* browser, TabStripUIEmbedder* embedder) {
  content::WebUI* const web_ui = TabStripUI::web_ui();
  DCHECK_EQ(Profile::FromWebUI(web_ui), browser->profile());
  browser_ = browser;
  embedder_ = embedder;
}

void TabStripUI::Deinitialize() {
  page_handler_.reset();
  DCHECK(browser_);
  DCHECK(embedder_);
  browser_ = nullptr;
  embedder_ = nullptr;
}

void TabStripUI::LayoutChanged() {
  if (page_handler_) {
    page_handler_->NotifyLayoutChanged();
  }
}

void TabStripUI::ReceivedKeyboardFocus() {
  if (page_handler_) {
    page_handler_->NotifyReceivedKeyboardFocus();
  }
}
