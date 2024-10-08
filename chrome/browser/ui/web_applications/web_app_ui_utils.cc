// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"

#include <optional>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

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
           .IsInstalled(*app_id)) {
    return std::nullopt;
  }

  return *app_id;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ShowAppManagementPageInAsh(const webapps::AppId& app_id) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::AppServiceProxy>()) {
    LOG(ERROR) << "AppServiceProxy not available.";
    return false;
  }
  service->GetRemote<crosapi::mojom::AppServiceProxy>()->ShowAppManagementPage(
      app_id);
  return true;
}
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::ShowAppManagementPage(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()), *app_id,
      ash::settings::AppManagementEntryPoint::kPageInfoView);
  return true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return ShowAppManagementPageInAsh(*app_id);
#else
  chrome::ShowWebAppSettings(chrome::FindBrowserWithTab(web_contents), *app_id,
                             AppSettingsPageEntryPoint::kPageInfoView);
  return true;
#endif
}

void OpenAppSettingsForParentApp(const webapps::AppId& parent_app_id,
                                 Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowAppManagementPageInAsh(parent_app_id);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowAppManagementPageInAsh(app_id);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::ShowAppManagementPage(
      profile, app_id, ash::settings::AppManagementEntryPoint::kSiteDataDialog);
#else
  chrome::ShowWebAppSettings(profile, app_id,
                             AppSettingsPageEntryPoint::kSiteDataDialog);
#endif
}

}  // namespace web_app
