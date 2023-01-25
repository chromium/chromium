// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/app_home_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/app_home_resources.h"
#include "chrome/grit/app_home_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace webapps {

namespace {

void AddAppHomeLocalizedStrings(content::WebUIDataSource* ui_source) {
  static constexpr webui::LocalizedString kAppHomeLocalizedStrings[] = {
      {"appHomeTitle", IDS_APP_HOME_TITLE},
      {"appWindowOpenLabel", IDS_APP_HOME_OPEN_IN_WINDOW},
      {"appWindowOpenCheckboxLabel",
       IDS_ACCNAME_APP_HOME_OPEN_IN_WINDOW_CHECKBOX},
      {"appLaunchAtStartupLabel", IDS_APP_HOME_LAUNCH_AT_STARTUP},
      {"appLaunchAtStartupCheckboxLabel",
       IDS_ACCNAME_APP_HOME_LAUNCH_AT_STARTUP_CHECKBOX},
      {"createShortcutForAppLabel", IDS_APP_HOME_CREATE_SHORTCUT},
      {"installLocallyLabel", IDS_APP_HOME_INSTALL_LOCALLY},
      {"uninstallAppLabel", IDS_APP_HOME_UNINSTALL_APP},
      {"appSettingsLabel", IDS_APP_HOME_APP_SETTINGS},
  };
  ui_source->AddLocalizedStrings(kAppHomeLocalizedStrings);
}

}  // namespace

AppHomeUI::AppHomeUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIAppLauncherPageHost);
  AddAppHomeLocalizedStrings(source);
  webui::SetupWebUIDataSource(
      source, base::make_span(kAppHomeResources, kAppHomeResourcesSize),
      IDR_APP_HOME_APP_HOME_HTML);
}

void AppHomeUI::BindInterface(
    mojo::PendingReceiver<app_home::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();

  page_factory_receiver_.Bind(std::move(receiver));
}

void AppHomeUI::CreatePageHandler(
    mojo::PendingRemote<app_home::mojom::Page> page,
    mojo::PendingReceiver<app_home::mojom::PageHandler> receiver) {
  DCHECK(page);
  Profile* profile = Profile::FromWebUI(web_ui());
  page_handler_ = std::make_unique<AppHomePageHandler>(
      web_ui(), profile, std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(AppHomeUI)

AppHomeUI::~AppHomeUI() = default;

}  // namespace webapps
