// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_ui.h"

#include <memory>

#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/common/trusted_types_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/floating_workspace_resources.h"
#include "chrome/grit/floating_workspace_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"
#include "ui/webui/webui_util.h"

namespace ash {

FloatingWorkspaceUIConfig::FloatingWorkspaceUIConfig()
    : ChromeOSWebUIConfig(content::kChromeUIScheme,
                          chrome::kChromeUIFloatingWorkspaceDialogHost) {}

FloatingWorkspaceUI::FloatingWorkspaceUI(content::WebUI* web_ui)
    : MojoWebDialogUI(web_ui) {
  auto main_handler = std::make_unique<FloatingWorkspaceDialogHandler>();
  main_handler_ = main_handler.get();
  web_ui->AddMessageHandler(std::move(main_handler));

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIFloatingWorkspaceDialogHost);

  webui::SetupWebUIDataSource(source, kFloatingWorkspaceResources,
                              IDR_FLOATING_WORKSPACE_FLOATING_WORKSPACE_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"floatingWorkspaceStartupDialogTitle",
       IDS_FLOATING_WORKSPACE_STARTUP_DIALOG_TITLE},
      {"floatingWorkspaceStartupDialogLongResponseTitle",
       IDS_FLOATING_WORKSPACE_STARTUP_DIALOG_LONG_RESPONSE_TITLE},
      {"floatingWorkspaceStartupDialogButton",
       IDS_FLOATING_WORKSPACE_STARTUP_DIALOG_BUTTON},
      {"floatingWorkspaceNetworkDialogTitle",
       IDS_FLOATING_WORKSPACE_NETWORK_DIALOG_TITLE},
      {"floatingWorkspaceNetworkDialogSubtitle",
       IDS_FLOATING_WORKSPACE_NETWORK_DIALOG_SUBTITLE},
      {"floatingWorkspaceErrorDialogTitle",
       IDS_FLOATING_WORKSPACE_ERROR_DIALOG_TITLE},
      {"floatingWorkspaceErrorDialogSubtitle",
       IDS_FLOATING_WORKSPACE_ERROR_DIALOG_SUBTITLE},
      {"floatingWorkspaceErrorDialogButton",
       IDS_FLOATING_WORKSPACE_ERROR_DIALOG_BUTTON},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  // Since we reuse animation from the OOBE consumer update screen, we
  // need to add label for the pause/play button. Also reuse a string for the
  // "Add WiFi" button on the network screen.
  static constexpr webui::LocalizedString kExtraStrings[] = {
      {"pauseAnimationAriaLabel", IDS_OOBE_PAUSE_ANIMATION_MESSAGE},
      {"playAnimationAriaLabel", IDS_OOBE_PLAY_ANIMATION_MESSAGE},
      {"addWiFiListItemName", IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME},
  };
  source->AddLocalizedStrings(kExtraStrings);

  // Add strings for the additional dialogs on the network screen.
  ui::network_element::AddLocalizedStrings(source);
  ui::network_element::AddOncLocalizedStrings(source);

  OobeUI::AddOobeComponents(source);
  ash::EnableTrustedTypesCSP(source);
}

FloatingWorkspaceUI::~FloatingWorkspaceUI() = default;

FloatingWorkspaceDialogHandler* FloatingWorkspaceUI::GetMainHandler() {
  return main_handler_;
}

void FloatingWorkspaceUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FloatingWorkspaceUI)
}  // namespace ash
