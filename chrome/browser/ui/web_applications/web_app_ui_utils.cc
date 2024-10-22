// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"

#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"

namespace web_app {

namespace {

std::optional<webapps::AppId> GetAppIdForManagementLinkInWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return std::nullopt;

  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id)
    return std::nullopt;

  if (!WebAppProvider::GetForWebApps(browser->profile())
           ->registrar_unsafe()
           .IsInstallState(*app_id,
                           {proto::INSTALLED_WITH_OS_INTEGRATION,
                            proto::INSTALLED_WITHOUT_OS_INTEGRATION})) {
    return std::nullopt;
  }

  return *app_id;
}

}  // namespace

bool GetLabelIdsForAppManagementLinkInPageInfo(
    content::WebContents* web_contents,
    int* link_text_id,
    int* tooltip_text_id) {
  std::optional<webapps::AppId> app_id =
      GetAppIdForManagementLinkInWebContents(web_contents);
  if (!app_id)
    return false;

  *link_text_id = IDS_WEB_APP_SETTINGS_LINK;
  *tooltip_text_id = IDS_WEB_APP_SETTINGS_LINK_TOOLTIP;
  return true;
}

bool HandleAppManagementLinkClickedInPageInfo(
    content::WebContents* web_contents) {
  std::optional<webapps::AppId> app_id =
      GetAppIdForManagementLinkInWebContents(web_contents);
  if (!app_id)
    return false;

#if BUILDFLAG(IS_CHROMEOS)
  chrome::ShowAppManagementPage(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()), *app_id,
      ash::settings::AppManagementEntryPoint::kPageInfoView);
  return true;
#else
  chrome::ShowWebAppSettings(chrome::FindBrowserWithTab(web_contents), *app_id,
                             AppSettingsPageEntryPoint::kPageInfoView);
  return true;
#endif
}

void OpenAppSettingsForParentApp(const webapps::AppId& parent_app_id,
                                 Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  chrome::ShowAppManagementPage(
      profile, parent_app_id,
      ash::settings::AppManagementEntryPoint::kSubAppsInstallPrompt);
#else
  chrome::ShowWebAppSettings(profile, parent_app_id,
                             AppSettingsPageEntryPoint::kSubAppsInstallPrompt);
#endif
}

void OpenAppSettingsForInstalledRelatedApp(const webapps::AppId& app_id,
                                           Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  chrome::ShowAppManagementPage(
      profile, app_id, ash::settings::AppManagementEntryPoint::kSiteDataDialog);
#else
  chrome::ShowWebAppSettings(profile, app_id,
                             AppSettingsPageEntryPoint::kSiteDataDialog);
#endif
}

}  // namespace web_app
