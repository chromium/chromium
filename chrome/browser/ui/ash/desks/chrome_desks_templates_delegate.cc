// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/chrome_desks_templates_delegate.h"

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/desks/chrome_desks_util.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace {

// Name for app not available toast.
constexpr char kAppNotAvailableTemplateToastName[] =
    "AppNotAvailableTemplateToast";

// Returns the TabStripModel that associates with `window` if the given `window`
// contains a browser frame, otherwise returns nullptr.
TabStripModel* GetTabstripModelForWindowIfAny(aura::Window* window) {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view ? browser_view->browser()->tab_strip_model() : nullptr;
}

// Returns the list of URLs that are open in `tab_strip_model`.
std::vector<GURL> GetURLsIfApplicable(TabStripModel* tab_strip_model) {
  DCHECK(tab_strip_model);

  std::vector<GURL> urls;
  for (int i = 0; i < tab_strip_model->count(); ++i)
    urls.push_back(tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  return urls;
}

// Return true if `app_id` is available to launch from template.
bool IsAppAvailable(const std::string& app_id,
                    apps::AppServiceProxy* app_service_proxy) {
  DCHECK(app_service_proxy);
  bool installed = false;
  Profile* app_profile = ProfileManager::GetActiveUserProfile();
  DCHECK(app_profile);

  app_service_proxy->AppRegistryCache().ForOneApp(
      app_id, [&](const apps::AppUpdate& app) {
        installed = apps_util::IsInstalled(app.Readiness());
      });
  if (installed)
    return true;
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(app_profile)
          ->GetInstalledExtension(app_id);
  return app != nullptr;
}

// Returns a vector of human readable unavailable app names from
// `desk_template`.
std::vector<std::u16string> GetUnavailableAppNames(
    const ash::DeskTemplate& desk_template) {
  const auto& launch_lists =
      desk_template.desk_restore_data()->app_id_to_launch_list();
  std::vector<std::u16string> app_names;
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  if (!app_service_proxy)
    return app_names;

  // Return the human readable app names of the unavailable apps on this device.
  for (const auto& [app_id, launch_list] : launch_lists) {
    if (launch_list.empty())
      continue;

    if (!IsAppAvailable(app_id, app_service_proxy)) {
      // `launch_list` is a list of windows associated with `app_id`, so we only
      // need the title of the first window.
      auto it = launch_list.begin();
      app_restore::AppRestoreData* app_restore_data = it->second.get();
      app_names.push_back(app_restore_data->title.value_or(u""));
    }
  }
  return app_names;
}

// Show unavailable app toast based on size of `unavailable_apps`.
void ShowUnavailableAppToast(
    const std::vector<std::u16string>& unavailable_apps) {
  std::u16string toast_string;
  switch (unavailable_apps.size()) {
    case 1:
      toast_string = l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_UNAVAILABLE_APP_TOAST_ONE,
          unavailable_apps[0]);
      break;
    case 2:
      toast_string = l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_UNAVAILABLE_APP_TOAST_TWO,
          unavailable_apps[0], unavailable_apps[1]);
      break;
    default:
      DCHECK_GT(unavailable_apps.size(), 2u);
      toast_string = l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_UNAVAILABLE_APP_TOAST_MORE,
          unavailable_apps[0], unavailable_apps[1],
          base::FormatNumber(unavailable_apps.size() - 2));
      break;
  }

  ash::ToastData toast_data = {/*id=*/kAppNotAvailableTemplateToastName,
                               ash::ToastCatalogName::kAppNotAvailable,
                               /*text=*/toast_string};
  ash::ToastManager::Get()->Show(std::move(toast_data));
}

// Creates a callback for when a favicon image is retrieved which creates a
// standard icon image and then calls `callback` with the standardized image.
// TODO(crbug.com/1318250): Remove this once non-lacros browser is not
// supported.
base::OnceCallback<void(const favicon_base::FaviconImageResult&)>
ImageResultToImageSkia(
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(const gfx::ImageSkia&)> image_skia_callback,
         const favicon_base::FaviconImageResult& result) {
        auto image = result.image.AsImageSkia();
        image.EnsureRepsForSupportedScales();
        std::move(image_skia_callback)
            .Run(apps::CreateStandardIconImage(image));
      },
      std::move(callback));
}

// Creates a callback for when a app icon image is retrieved which creates a
// standard icon image and then calls `callback` with the standardized image.
base::OnceCallback<void(apps::IconValuePtr icon_value)>
AppIconResultToImageSkia(
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(const gfx::ImageSkia&)> image_skia_callback,
         apps::IconValuePtr icon_value) {
        auto image = icon_value->uncompressed;
        image.EnsureRepsForSupportedScales();
        std::move(image_skia_callback)
            .Run(apps::CreateStandardIconImage(image));
      },
      std::move(callback));
}

}  // namespace

ChromeDesksTemplatesDelegate::ChromeDesksTemplatesDelegate() = default;

ChromeDesksTemplatesDelegate::~ChromeDesksTemplatesDelegate() = default;

void ChromeDesksTemplatesDelegate::GetAppLaunchDataForDeskTemplate(
    aura::Window* window,
    GetAppLaunchDataCallback callback) const {
  DCHECK(callback);

  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(active_user);
  Profile* user_profile =
      ash::ProfileHelper::Get()->GetProfileByUser(active_user);
  if (!user_profile) {
    std::move(callback).Run({});
    return;
  }

  if (!IsWindowSupportedForDeskTemplate(window)) {
    std::move(callback).Run({});
    return;
  }

  // Get `full_restore_data` from FullRestoreSaveHandler which contains all
  // restoring information for all apps running on the device.
  const app_restore::RestoreData* full_restore_data =
      full_restore::FullRestoreSaveHandler::GetInstance()->GetRestoreData(
          user_profile->GetPath());
  DCHECK(full_restore_data);

  const std::string app_id = full_restore::GetAppId(window);
  DCHECK(!app_id.empty());

  auto& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(user_profile)
          ->AppRegistryCache();
  const auto app_type = app_registry_cache.GetAppType(app_id);

  // Get the window id needed to fetch app launch info. For chrome apps in
  // lacros, the window id needs to be fetched from the `LacrosSaveHandler`. See
  // https://crbug.com/1335491 for more details.
  const int32_t window_id =
      app_type == apps::AppType::kStandaloneBrowserChromeApp
          ? full_restore::FullRestoreSaveHandler::GetInstance()
                ->GetLacrosChromeAppWindowId(window)
          : window->GetProperty(app_restore::kWindowIdKey);

  auto app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id);

  const std::string* app_name =
      window->GetProperty(app_restore::kBrowserAppNameKey);
  if (app_name)
    app_launch_info->app_name = *app_name;

  // Read all other relevant app launching information from `app_restore_data`
  // to `app_launch_info`.
  const app_restore::AppRestoreData* app_restore_data =
      full_restore_data->GetAppRestoreData(app_id, window_id);
  if (app_restore_data) {
    app_launch_info->app_type_browser = app_restore_data->app_type_browser;
    app_launch_info->event_flag = app_restore_data->event_flag;
    app_launch_info->container = app_restore_data->container;
    app_launch_info->disposition = app_restore_data->disposition;
    app_launch_info->file_paths = app_restore_data->file_paths;
    if (app_restore_data->intent) {
      app_launch_info->intent = app_restore_data->intent->Clone();
    }
  }

  if (app_id != app_constants::kChromeAppId &&
      app_id != app_constants::kLacrosAppId &&
      (app_type == apps::AppType::kChromeApp ||
       app_type == apps::AppType::kStandaloneBrowserChromeApp ||
       app_type == apps::AppType::kWeb)) {
    // If these values are not present, we will not be able to restore the
    // application. See http://crbug.com/1232520 for more information.
    if (!app_launch_info->container.has_value() ||
        !app_launch_info->disposition.has_value()) {
      std::move(callback).Run({});
      return;
    }
  }

  auto* tab_strip_model = GetTabstripModelForWindowIfAny(window);
  if (tab_strip_model) {
    app_launch_info->urls = GetURLsIfApplicable(tab_strip_model);
    app_launch_info->active_tab_index = tab_strip_model->active_index();
    int index_of_first_non_pinned_tab =
        tab_strip_model->IndexOfFirstNonPinnedTab();
    // Only set this field if there are pinned tabs. `IndexOfFirstNonPinnedTab`
    // returns 0 if there are no pinned tabs.
    if (index_of_first_non_pinned_tab > 0 &&
        index_of_first_non_pinned_tab <= tab_strip_model->count()) {
      app_launch_info->first_non_pinned_tab_index =
          index_of_first_non_pinned_tab;
    }
    if (tab_strip_model->SupportsTabGroups()) {
      app_launch_info->tab_group_infos =
          chrome_desks_util::ConvertTabGroupsToTabGroupInfos(
              tab_strip_model->group_model());
    }
    std::move(callback).Run(std::move(app_launch_info));
    return;
  }

  if (app_id == app_constants::kLacrosAppId) {
    // TODO(avynn): Add lacros support for tab groups.
    const std::string* lacros_window_id =
        window->GetProperty(app_restore::kLacrosWindowId);
    DCHECK(lacros_window_id);
    const_cast<ChromeDesksTemplatesDelegate*>(this)->GetLacrosChromeInfo(
        std::move(callback), *lacros_window_id, std::move(app_launch_info));
    return;
  }

  std::move(callback).Run(std::move(app_launch_info));
}

desks_storage::DeskModel* ChromeDesksTemplatesDelegate::GetDeskModel() {
  return DesksClient::Get()->GetDeskModel();
}

bool ChromeDesksTemplatesDelegate::IsIncognitoWindow(
    aura::Window* window) const {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view && browser_view->GetIncognito();
}

absl::optional<gfx::ImageSkia>
ChromeDesksTemplatesDelegate::MaybeRetrieveIconForSpecialIdentifier(
    const std::string& identifier,
    const ui::ColorProvider* color_provider) const {
  if (identifier == chrome::kChromeUINewTabURL) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    return absl::make_optional<gfx::ImageSkia>(apps::CreateStandardIconImage(
        rb.GetImageNamed(IDR_PRODUCT_LOGO_32).AsImageSkia()));
  } else if (identifier == ash::DeskTemplate::kIncognitoWindowIdentifier) {
    DCHECK(color_provider);
    return apps::CreateStandardIconImage(
        ui::ThemedVectorIcon(
            ui::ImageModel::FromVectorIcon(kIncognitoProfileIcon,
                                           ui::kColorAvatarIconIncognito)
                .GetVectorIcon())
            .GetImageSkia(color_provider));
  }

  return absl::nullopt;
}

void ChromeDesksTemplatesDelegate::GetFaviconForUrl(
    const std::string& page_url,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback,
    base::CancelableTaskTracker* tracker) const {
  // Get the icons from lacros favicon service.
  if (crosapi::browser_util::IsLacrosPrimaryBrowser()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->desk_template_ash()
        ->GetFaviconImage(GURL(page_url), std::move(callback));
    return;
  }

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile(),
          ServiceAccessType::EXPLICIT_ACCESS);

  favicon_service->GetFaviconImageForPageURL(
      GURL(page_url), ImageResultToImageSkia(std::move(callback)), tracker);
}

void ChromeDesksTemplatesDelegate::GetIconForAppId(
    const std::string& app_id,
    int desired_icon_size,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) const {
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  if (!app_service_proxy) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  app_service_proxy->LoadIcon(
      app_service_proxy->AppRegistryCache().GetAppType(app_id), app_id,
      apps::IconType::kStandard, desired_icon_size,
      /*allow_placeholder_icon=*/false,
      AppIconResultToImageSkia(std::move(callback)));
}

bool ChromeDesksTemplatesDelegate::IsAppAvailable(
    const std::string& app_id) const {
  Profile* app_profile = ProfileManager::GetActiveUserProfile();
  DCHECK(app_profile);

  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(app_profile);
  DCHECK(app_service_proxy);

  return ::IsAppAvailable(app_id, app_service_proxy);
}

void ChromeDesksTemplatesDelegate::LaunchAppsFromTemplate(
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  std::vector<std::u16string> unavailable_apps =
      GetUnavailableAppNames(*desk_template);
  // Show app unavailable toast.
  if (!unavailable_apps.empty())
    ShowUnavailableAppToast(unavailable_apps);
  DesksClient::Get()->LaunchAppsFromTemplate(std::move(desk_template));
}

// Returns true if `window` is supported in desk templates feature.
bool ChromeDesksTemplatesDelegate::IsWindowSupportedForDeskTemplate(
    aura::Window* window) const {
  if (!ash::DeskTemplate::IsAppTypeSupported(window))
    return false;

  // Exclude incognito browser window.
  return !IsIncognitoWindow(window);
}

std::string ChromeDesksTemplatesDelegate::GetAppShortName(
    const std::string& app_id) {
  std::string name;
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  DCHECK(app_service_proxy);

  app_service_proxy->AppRegistryCache().ForOneApp(
      app_id,
      [&name](const apps::AppUpdate& update) { name = update.ShortName(); });
  return name;
}

void ChromeDesksTemplatesDelegate::OnLacrosChromeInfoReturned(
    GetAppLaunchDataCallback callback,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info,
    crosapi::mojom::DeskTemplateStatePtr state) {
  app_launch_info->urls = state->urls;
  app_launch_info->active_tab_index = state->active_index;
  std::move(callback).Run(std::move(app_launch_info));
}

void ChromeDesksTemplatesDelegate::GetLacrosChromeInfo(
    GetAppLaunchDataCallback callback,
    const std::string& window_unique_id,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info) {
  crosapi::BrowserManager* browser_manager = crosapi::BrowserManager::Get();
  if (!browser_manager || !browser_manager->IsRunning()) {
    LOG(WARNING)
        << "The browser manager is not running.  Cannot request browser state.";
    std::move(callback).Run({});
    return;
  }

  browser_manager->GetBrowserInformation(
      window_unique_id,
      base::BindOnce(&ChromeDesksTemplatesDelegate::OnLacrosChromeInfoReturned,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(app_launch_info)));
}
