// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_settings/web_app_settings_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/app_settings_resources.h"
#include "chrome/grit/app_settings_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

void AddAppManagementStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"title", IDS_WEB_APP_SETTINGS_TITLE},
      {"appManagementNotificationsLabel", IDS_APP_MANAGEMENT_NOTIFICATIONS},
      {"appManagementPermissionsLabel", IDS_APP_MANAGEMENT_PERMISSIONS},
      {"appManagementLocationPermissionLabel", IDS_APP_MANAGEMENT_LOCATION},
      {"appManagementMicrophonePermissionLabel", IDS_APP_MANAGEMENT_MICROPHONE},
      {"appManagementMorePermissionsLabel", IDS_APP_MANAGEMENT_MORE_SETTINGS},
      {"appManagementCameraPermissionLabel", IDS_APP_MANAGEMENT_CAMERA},
      {"appManagementUninstallLabel", IDS_APP_MANAGEMENT_UNINSTALL_APP},
      {"appManagementWindowModeLabel", IDS_APP_MANAGEMENT_WINDOW},
      {"appManagementRunOnOsLoginModeLabel",
       IDS_APP_MANAGEMENT_RUN_ON_OS_LOGIN},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

WebAppSettingsUI::WebAppSettingsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  // Set up the chrome://app-settings source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIWebAppSettingsHost);

  AddAppManagementStrings(html_source);

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kAppSettingsResources, kAppSettingsResourcesSize),
      IDR_APP_SETTINGS_WEB_APP_SETTINGS_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);
}

void WebAppSettingsUI::BindInterface(
    mojo::PendingReceiver<app_management::mojom::PageHandlerFactory> receiver) {
  if (!app_management_page_handler_factory_) {
    app_management_page_handler_factory_ =
        std::make_unique<AppManagementPageHandlerFactory>(
            Profile::FromWebUI(web_ui()));
  }
  app_management_page_handler_factory_->Bind(std::move(receiver));
}

WebAppSettingsUI::~WebAppSettingsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebAppSettingsUI)
