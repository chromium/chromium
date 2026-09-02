// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/buildflag.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/install_banner_config.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_elider.h"
#include "ui/menus/simple_menu_model.h"

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

std::u16string GetInstallPWALabel(BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel()) {
    return std::u16string();
  }
  // There may be no active web contents in tests.
  auto* const web_contents =
      browser->GetTabStripModel()->GetActiveWebContents();
  if (!web_contents) {
    return std::u16string();
  }
  if (!web_app::CanCreateWebApp(browser)) {
    return std::u16string();
  }
  // Don't allow apps created from chrome-extension urls.
  if (web_contents->GetLastCommittedURL().SchemeIs("chrome-extension")) {
    return std::u16string();
  }

  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  web_app::WebAppProvider* const provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(browser->GetProfile());
  if (provider && app_id &&
      provider->registrar_unsafe().GetInstallState(*app_id) ==
          web_app::proto::INSTALLED_WITH_OS_INTEGRATION &&
      provider->registrar_unsafe().GetAppUserDisplayMode(*app_id) !=
          web_app::mojom::UserDisplayMode::kBrowser) {
    return std::u16string();
  }

  std::u16string install_page_as_app_label =
      l10n_util::GetStringUTF16(IDS_INSTALL_DIY_TO_OS_LAUNCH_SURFACE);
  webapps::AppBannerManager* banner =
      webapps::AppBannerManager::FromWebContents(web_contents);
  if (!banner) {
    // Showing `Install Page as App` allows the user to refetch the manifest and
    // go through the install flow without relying on the AppBannerManager to
    // finish working.
    return install_page_as_app_label;
  }

  std::optional<webapps::InstallBannerConfig> install_config =
      banner->GetCurrentBannerConfig();
  if (!install_config) {
    // In some edge cases where the `AppBannerManager` pipeline hasn't run yet,
    // the information populated to be used for determining installability and
    // other parameters is not available. In this case, allow users to try
    // installability by refetching the manifest.
    return install_page_as_app_label;
  }
  CHECK_EQ(install_config->mode, webapps::AppBannerMode::kWebApp);
  webapps::InstallableWebAppCheckResult installable =
      banner->GetInstallableWebAppCheckResult();

  switch (installable) {
    case webapps::InstallableWebAppCheckResult::kUnknown:
      // Loading of the menu model is synchronous, so there could be a condition
      // where the `AppBannerManager` has not yet finished the pipeline while
      // the menu item has been triggered. In such a case,
      // `banner->GetInstallableWebAppCheckResult()` returns the default value
      // of `kUnknown`.
      // Show `Install Page as App` for that use-case, since that allows the
      // user to trigger the install flow to verify all the data required for
      // installability. The correct dialog will be shown to the user depending
      // on whether the app turns out to be installable or not.
      return install_page_as_app_label;
    case webapps::InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      // Returning an empty string here allows the `launch page as app` field to
      // get populated in place of the `install` strings.
      return std::u16string();
    case webapps::InstallableWebAppCheckResult::kNo:
      return install_page_as_app_label;
    case webapps::InstallableWebAppCheckResult::kYes_ByUserRequest:
    case webapps::InstallableWebAppCheckResult::kYes_Promotable: {
      std::u16string app_name = install_config->GetWebOrNativeAppName();
      if (app_name.empty()) {
        // Prefer showing `Install Page as App` here, as users can set the name
        // of the installed app on the DIY app dialog anyway.
        return install_page_as_app_label;
      }
      return l10n_util::GetStringFUTF16(
          IDS_INSTALL_TO_OS_LAUNCH_SURFACE,
          ui::EscapeMenuLabelAmpersands(app_name));
    }
  }
}

ui::ImageModel GetInstallPWAIcon(BrowserWindowInterface* browser) {
  ui::ImageModel app_icon_to_use = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kInstallDesktopIcon
                                        : kInstallDesktopChromeRefreshOldIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  if (!browser || !browser->GetTabStripModel()) {
    return app_icon_to_use;
  }
  content::WebContents* const web_contents =
      browser->GetTabStripModel()->GetActiveWebContents();
  if (!web_contents) {
    return app_icon_to_use;
  }

  webapps::AppBannerManager* const banner =
      webapps::AppBannerManager::FromWebContents(web_contents);
  if (!banner) {
    return app_icon_to_use;
  }

  // For sites that are not installable (DIY apps), do not return any icons,
  // instead use the default chrome refresh icon for installing.
  auto installable_check_result = banner->GetInstallableWebAppCheckResult();
  if (installable_check_result == webapps::InstallableWebAppCheckResult::kNo ||
      installable_check_result ==
          webapps::InstallableWebAppCheckResult::kUnknown) {
    return app_icon_to_use;
  }

  std::optional<webapps::WebAppBannerData> install_config =
      banner->GetCurrentWebAppBannerData();

  // If no data or no icons have been obtained by the AppBannerManager, return
  // the default icon.
  if (!install_config || install_config->primary_icon.empty()) {
    return app_icon_to_use;
  }

  gfx::ImageSkia primary_icon =
      gfx::ImageSkia::CreateFrom1xBitmap(install_config->primary_icon);
  gfx::ImageSkia resized_app_icon =
      gfx::ImageSkiaOperations::CreateResizedImage(
          primary_icon, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(ui::SimpleMenuModel::kDefaultIconSize,
                    ui::SimpleMenuModel::kDefaultIconSize));
  app_icon_to_use = ui::ImageModel::FromImageSkia(resized_app_icon);
  return app_icon_to_use;
}

std::u16string GetOpenPWALabel(BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel() ||
      !browser->GetTabStripModel()->GetActiveWebContents()) {
    return std::u16string();
  }
  std::optional<webapps::AppId> app_id =
      web_app::GetWebAppForActiveTab(browser);
  if (!app_id.has_value()) {
    return std::u16string();
  }

  // Only show this menu item for apps that open in an app window.
  const auto* const provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(browser->GetProfile());
  if (!provider || provider->registrar_unsafe().GetAppUserDisplayMode(
                       *app_id) == web_app::mojom::UserDisplayMode::kBrowser) {
    return std::u16string();
  }

  const std::u16string short_name =
      base::UTF8ToUTF16(provider->registrar_unsafe().GetAppShortName(*app_id));
  return l10n_util::GetStringFUTF16(
      IDS_OPEN_IN_APP_WINDOW,
      ui::EscapeMenuLabelAmpersands(gfx::TruncateString(
          short_name,
          GetLayoutConstant(LayoutConstant::kAppMenuMaximumCharacterLength),
          gfx::CHARACTER_BREAK)));
}

}  // namespace web_app
