// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/app_home/app_home_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/app_home_resources.h"
#include "chrome/grit/app_home_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/resource/resource_bundle.h"

bool AppHomeUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile &&
         extensions::ExtensionSystem::Get(profile)->extension_service() &&
         !profile->IsGuestSession();
}

std::unique_ptr<content::WebUIController>
AppHomeUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                       const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUIAppLauncherPageHost);
  }
  return std::make_unique<webapps::AppHomeUI>(web_ui);
}

namespace webapps {

namespace {

void AddAppHomeLocalizedStrings(content::WebUIDataSource* ui_source) {
  static constexpr webui::LocalizedString kAppHomeLocalizedStrings[] = {
      {"appHomeTitle", IDS_APP_HOME_TITLE},
      {"appWindowOpenLabel", IDS_APP_HOME_OPEN_IN_WINDOW},
      {"appLaunchAtStartupLabel", IDS_APP_HOME_LAUNCH_AT_STARTUP},
      {"createShortcutForAppLabel", IDS_APP_HOME_CREATE_SHORTCUT},
      {"installLocallyLabel", IDS_APP_HOME_INSTALL_LOCALLY},
      {"uninstallAppLabel", IDS_APP_HOME_UNINSTALL_APP},
      {"removeAppLabel", IDS_APP_HOME_REMOVE_APP},
      {"appSettingsLabel", IDS_APP_HOME_APP_SETTINGS},
      {"viewInWebStore", IDS_NEW_TAB_APP_DETAILS},
      {"notInstalled", IDS_ACCNAME_APP_HOME_NOT_INSTALLED},
      {"appAppearanceLabel", IDS_APP_HOME_APP_NO_APPS},
      {"learnToInstall", IDS_APP_HOME_APP_LEARN_INSTALL}};
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

// static
base::RefCountedMemory* AppHomeUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_BOOKMARK_BAR_APPS_SHORTCUT, scale_factor);
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
