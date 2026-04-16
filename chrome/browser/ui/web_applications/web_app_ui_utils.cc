// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"

#include <optional>

#include "base/memory/weak_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/web_app_id.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/check_deref.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/user_manager/user.h"
#endif

namespace web_app {

namespace {

std::optional<webapps::AppId> GetAppIdForManagementLinkInWebContents(
    content::WebContents* web_contents) {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (!browser) {
    return std::nullopt;
  }

  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id) {
    return std::nullopt;
  }

  if (!WebAppProvider::GetForWebApps(browser->GetProfile())
           ->registrar_unsafe()
           .AppMatches(*app_id, WebAppFilter::InstalledInChrome())) {
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
  if (!app_id) {
    return false;
  }

  *link_text_id = IDS_WEB_APP_SETTINGS_LINK;
  *tooltip_text_id = IDS_WEB_APP_SETTINGS_LINK_TOOLTIP;
  return true;
}

bool HandleAppManagementLinkClickedInPageInfo(
    content::WebContents* web_contents) {
  std::optional<webapps::AppId> app_id =
      GetAppIdForManagementLinkInWebContents(web_contents);
  if (!app_id) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
          web_contents->GetBrowserContext());
  // TODO: Remove the if stmt, and replace it by CHECK().
  // This method is called only by clicking the "Site setting" or "App setting"
  // option from the Page Info bubble, which is shown from the browser's
  // omnibox. Theoretically a shimless RMA profile may have an app. But shimless
  // RMA screen is full-screen and has no omnibox.
  if (!user) {
    return false;
  }
  ash::SettingsAppManager::Get()->Open(
      *user,
      ash::SettingsAppManager::OpenParams{
          .sub_page =
              ash::SettingsAppManager::CreateAppManagementPagePath(*app_id),
          .entry_point = ash::SettingsAppManager::EntryPoint::kPageInfoView});
  return true;
#else
  chrome::ShowWebAppSettings(
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents),
      *app_id, AppSettingsPageEntryPoint::kPageInfoView);
  return true;
#endif
}

void OpenAppSettingsForParentApp(const webapps::AppId& parent_app_id,
                                 base::WeakPtr<Profile> profile) {
  if (!profile) {
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile.get());
  // TODO: Remove the if stmt, and replace it by CHECK().
  // The function OpenAppSettingsForParentApp is bound as a callback to the
  // "Manage" link in the Sub Apps Install dialog. This dialog is only triggered
  // when a parent Web App tries to install its Sub App.
  // The Web App can be enabled not only on user profiles but also on shimless
  // RMA profiles, so this method may get a shimless RMA profile.
  if (!user) {
    return;
  }
  ash::SettingsAppManager::Get()->Open(
      *user,
      ash::SettingsAppManager::OpenParams{
          .sub_page = ash::SettingsAppManager::CreateAppManagementPagePath(
              parent_app_id),
          .entry_point =
              ash::SettingsAppManager::EntryPoint::kSubAppsInstallPrompt});
#else
  chrome::ShowWebAppSettings(profile.get(), parent_app_id,
                             AppSettingsPageEntryPoint::kSubAppsInstallPrompt);
#endif
}

void OpenAppSettingsForInstalledRelatedApp(const webapps::AppId& app_id,
                                           Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  // TODO: Remove the if stmt, and replace it by CHECK().
  // This method is called only from PageSpecificSiteDataDialog, which is
  // accessed by clicking the "Site Data" or "Cookies" option from the Page Info
  // bubble, which is shown from the browser's omnibox.
  if (!user) {
    return;
  }
  ash::SettingsAppManager::Get()->Open(
      *user,
      ash::SettingsAppManager::OpenParams{
          .sub_page =
              ash::SettingsAppManager::CreateAppManagementPagePath(app_id),
          .entry_point = ash::SettingsAppManager::EntryPoint::kSiteDataDialog});
#else
  chrome::ShowWebAppSettings(profile, app_id,
                             AppSettingsPageEntryPoint::kSiteDataDialog);
#endif
}

}  // namespace web_app
