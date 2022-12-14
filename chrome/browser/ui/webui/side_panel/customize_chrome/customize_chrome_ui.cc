// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

CustomizeChromeUI::CustomizeChromeUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()),
      page_factory_receiver_(this) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUICustomizeChromeSidePanelHost);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"classicChrome", IDS_NTP_CUSTOMIZE_NO_BACKGROUND_LABEL},
      {"colorsContainerLabel", IDS_NTP_THEMES_CONTAINER_LABEL},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"customizeThisPage", IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL},
      {"appearanceHeader", IDS_NTP_CUSTOMIZE_APPEARANCE_LABEL},
      {"defaultColorName", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"mostVisited", IDS_NTP_CUSTOMIZE_MOST_VISITED_LABEL},
      {"myShortcuts", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_LABEL},
      {"shortcutsCurated", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_DESC},
      {"shortcutsHeader", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      {"shortcutsSuggested", IDS_NTP_CUSTOMIZE_MOST_VISITED_DESC},
      {"showShortcutsToggle", IDS_NTP_CUSTOMIZE_SHOW_SHORTCUTS_LABEL},
      {"title", IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE},
      {"uploadedImage", IDS_NTP_CUSTOMIZE_UPLOADED_IMAGE_LABEL},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSidePanelCustomizeChromeResources,
                      kSidePanelCustomizeChromeResourcesSize),
      IDR_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_HTML);

  content::WebUIDataSource::Add(profile_, source);
}

CustomizeChromeUI::~CustomizeChromeUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CustomizeChromeUI)

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandlerFactory>
        receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  page_factory_receiver_.Bind(std::move(receiver));
}

void CustomizeChromeUI::CreatePageHandler(
    mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  customize_chrome_page_handler_ = std::make_unique<CustomizeChromePageHandler>(
      std::move(pending_page_handler), std::move(pending_page),
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_),
      web_contents_);
}
