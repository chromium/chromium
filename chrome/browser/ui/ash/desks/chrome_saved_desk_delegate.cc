// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/chrome_saved_desk_delegate.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
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
#include "chrome/browser/ui/ash/desks/admin_template_service_factory.h"
#include "chrome/browser/ui/ash/desks/chrome_desks_util.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
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
#include "components/desks_storage/core/admin_template_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/core/window_properties.h"
#include "url/gurl.h"

namespace {

// Returns the TabStripModel that associates with `window` if the given `window`
// contains a browser frame, otherwise returns nullptr.
TabStripModel* GetTabstripModelForWindowIfAny(aura::Window* window) {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view ? browser_view->browser()->tab_strip_model() : nullptr;
}

// Returns the list of URLs that are open in `tab_strip_model`.
std::vector<GURL> GetURLsIfApplicable(TabStripModel& tab_strip_model) {
  std::vector<GURL> urls;
  for (int i = 0; i < tab_strip_model.count(); ++i) {
    urls.push_back(tab_strip_model.GetWebContentsAt(i)->GetLastCommittedURL());
  }
  return urls;
}

// Return true if `app_id` is available to launch from saved desk.
bool IsAppAvailable(apps::AppServiceProxy& app_service_proxy,
                    const std::string& app_id) {
  bool installed = false;
  Profile* app_profile = ProfileManager::GetActiveUserProfile();
  DCHECK(app_profile);

  app_service_proxy.AppRegistryCache().ForOneApp(
      app_id, [&](const apps::AppUpdate& app) {
        installed = apps_util::IsInstalled(app.Readiness());
      });
  if (installed) {
    return true;
  }
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(app_profile)
          ->GetInstalledExtension(app_id);
  return app != nullptr;
}

// Updates `out_app_names` with titles of browser apps that aren't available
// on this device.  This is confusingly determined by resolving the `app_name`
// field to an app ID and running said ID through `IsAppAvailable`.  If the
// app is unavailable we append the app_title to `out_app_names`.
void GetUnavailableBrowserAppNames(
    apps::AppServiceProxy& app_service_proxy,
    const app_restore::RestoreData::LaunchList& launch_list,
    std::vector<std::u16string>& out_app_names) {
  for (const auto& [id, restore_data] : launch_list) {
    if (restore_data->browser_extra_info.app_type_browser.value_or(false) &&
        restore_data->browser_extra_info.app_name.has_value()) {
      std::string app_id = app_restore::GetAppIdFromAppName(
          restore_data->browser_extra_info.app_name.value());
      if (!IsAppAvailable(app_service_proxy, app_id)) {
        out_app_names.push_back(
            restore_data->window_info.app_title.value_or(u""));
      }
    }
  }
}

// Returns a vector of human readable unavailable app names from
// `desk_template`.
std::vector<std::u16string> GetUnavailableAppNames(
    const ash::DeskTemplate& saved_desk) {
  const auto& launch_lists =
      saved_desk.desk_restore_data()->app_id_to_launch_list();
  std::vector<std::u16string> app_names;
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  if (!app_service_proxy) {
    return app_names;
  }

  // Return the human readable app names of the unavailable apps on this device.
  for (const auto& [app_id, launch_list] : launch_lists) {
    if (launch_list.empty()) {
      continue;
    }

    // If the app ID is a browser then we need to iterate through its windows
    // to catch uninstalled PWAs.
    if (app_id == app_constants::kChromeAppId ||
        app_id == app_constants::kLacrosAppId) {
      GetUnavailableBrowserAppNames(*app_service_proxy, launch_list, app_names);
    }

    if (!IsAppAvailable(*app_service_proxy, app_id)) {
      // `launch_list` is a list of windows associated with `app_id`, so we only
      // need the title of the first window.
      auto it = launch_list.begin();
      app_restore::AppRestoreData* app_restore_data = it->second.get();
      app_names.push_back(
          app_restore_data->window_info.app_title.value_or(u""));
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

  ash::ToastData toast_data = {
      /*id=*/chrome_desks_util::kAppNotAvailableTemplateToastName,
      ash::ToastCatalogName::kAppNotAvailable,
      /*text=*/toast_string};
  ash::ToastManager::Get()->Show(std::move(toast_data));
}

// Creates a standard icon image via `result`, and then calls `callback` with
// the standardized image.
// TODO(crbug.com/1318250): Remove this once non-lacros browser is not
// supported.
void ImageResultToImageSkia(
    base::OnceCallback<void(const gfx::ImageSkia&)> callback,
    const favicon_base::FaviconRawBitmapResult& result) {
  TRACE_EVENT0("ui", "chrome_saved_desk_delegate::ImageResultToImageSkia");
  if (!result.is_valid()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  auto image =
      gfx::Image::CreateFrom1xPNGBytes(result.bitmap_data).AsImageSkia();
  image.EnsureRepsForSupportedScales();
  std::move(callback).Run(apps::CreateStandardIconImage(image));
}

// Creates a callback for when a app icon image is retrieved which creates a
// standard icon image and then calls `callback` with the standardized image.
base::OnceCallback<void(apps::IconValuePtr icon_value)>
AppIconResultToImageSkia(
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
  TRACE_EVENT0("ui", "chrome_saved_desk_delegate::AppIconResultToImageSkia");
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

ChromeSavedDeskDelegate::ChromeSavedDeskDelegate() = default;

ChromeSavedDeskDelegate::~ChromeSavedDeskDelegate() = default;

void ChromeSavedDeskDelegate::GetAppLaunchDataForSavedDesk(
    aura::Window* window,
    GetAppLaunchDataCallback callback) const {
  TRACE_EVENT0("ui", "ChromeSavedDeskDelegate::GetAppLaunchDataForSavedDesk");
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

  if (!IsWindowSupportedForSavedDesk(window)) {
    std::move(callback).Run({});
    return;
  }

  // Get `full_restore_data` from FullRestoreSaveHandler which contains all
  // restoring information for all apps running on the device.
  const app_restore::RestoreData* full_restore_data =
      full_restore::FullRestoreSaveHandler::GetInstance()->GetRestoreData(
          user_profile->GetPath());
  if (!full_restore_data) {
    std::move(callback).Run({});
    return;
  }

  const std::string app_id = full_restore::GetAppId(window);
  // TODO: b/296445956 - Implement a long term fix for saving the arc ghost
  // window.
  if (app_id.empty()) {
    std::move(callback).Run({});
    return;
  }

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

  if (const std::string* app_name =
          window->GetProperty(app_restore::kBrowserAppNameKey)) {
    app_launch_info->browser_extra_info.app_name = *app_name;
  }

  // Read all other relevant app launching information from `app_restore_data`
  // to `app_launch_info`.
  const app_restore::AppRestoreData* app_restore_data =
      full_restore_data->GetAppRestoreData(app_id, window_id);
  if (app_restore_data) {
    app_launch_info->event_flag = app_restore_data->event_flag;
    app_launch_info->container = app_restore_data->container;
    app_launch_info->disposition = app_restore_data->disposition;
    app_launch_info->file_paths = app_restore_data->file_paths;
    app_launch_info->override_url = app_restore_data->override_url;
    if (app_restore_data->intent) {
      app_launch_info->intent = app_restore_data->intent->Clone();
    }
    app_launch_info->browser_extra_info.app_type_browser =
        app_restore_data->browser_extra_info.app_type_browser;
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

  if (auto* tab_strip_model = GetTabstripModelForWindowIfAny(window)) {
    app_launch_info->browser_extra_info.urls =
        GetURLsIfApplicable(*tab_strip_model);
    app_launch_info->browser_extra_info.active_tab_index =
        tab_strip_model->active_index();
    int index_of_first_non_pinned_tab =
        tab_strip_model->IndexOfFirstNonPinnedTab();
    // Only set this field if there are pinned tabs. `IndexOfFirstNonPinnedTab`
    // returns 0 if there are no pinned tabs.
    if (index_of_first_non_pinned_tab > 0 &&
        index_of_first_non_pinned_tab <= tab_strip_model->count()) {
      app_launch_info->browser_extra_info.first_non_pinned_tab_index =
          index_of_first_non_pinned_tab;
    }
    if (tab_strip_model->SupportsTabGroups()) {
      app_launch_info->browser_extra_info.tab_group_infos =
          chrome_desks_util::ConvertTabGroupsToTabGroupInfos(
              tab_strip_model->group_model());
    }
    std::move(callback).Run(std::move(app_launch_info));
    return;
  }

  if (app_id == app_constants::kLacrosAppId) {
    const std::string* lacros_window_id =
        window->GetProperty(app_restore::kLacrosWindowId);
    DCHECK(lacros_window_id);
    GetLacrosChromeInfo(std::move(callback), *lacros_window_id,
                        std::move(app_launch_info));
    return;
  }

  std::move(callback).Run(std::move(app_launch_info));
}

desks_storage::DeskModel* ChromeSavedDeskDelegate::GetDeskModel() {
  return DesksClient::Get()->GetDeskModel();
}

desks_storage::AdminTemplateService*
ChromeSavedDeskDelegate::GetAdminTemplateService() {
  return ash::AdminTemplateServiceFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
}

bool ChromeSavedDeskDelegate::IsWindowPersistable(aura::Window* window) const {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return !(browser_view && browser_view->GetIncognito()) &&
         window->GetProperty(wm::kPersistableKey);
}

std::optional<gfx::ImageSkia>
ChromeSavedDeskDelegate::MaybeRetrieveIconForSpecialIdentifier(
    const std::string& identifier,
    const ui::ColorProvider* color_provider) const {
  TRACE_EVENT0(
      "ui", "ChromeSavedDeskDelegate::MaybeRetrieveIconForSpecialIdentifier");
  if (identifier == chrome::kChromeUINewTabURL) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    return apps::CreateStandardIconImage(
        rb.GetImageNamed(IDR_PRODUCT_LOGO_32).AsImageSkia());
  } else if (identifier == ash::DeskTemplate::kIncognitoWindowIdentifier) {
    DCHECK(color_provider);
    gfx::ImageSkia icon =
        ui::ThemedVectorIcon(
            ui::ImageModel::FromVectorIcon(kIncognitoProfileIcon,
                                           ui::kColorAvatarIconIncognito)
                .GetVectorIcon())
            .GetImageSkia(color_provider);
    icon.EnsureRepsForSupportedScales();
    return apps::CreateStandardIconImage(icon);
  }

  return std::nullopt;
}

void ChromeSavedDeskDelegate::GetFaviconForUrl(
    const std::string& page_url,
    uint64_t lacros_profile_id,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback,
    base::CancelableTaskTracker* tracker) const {
  TRACE_EVENT0("ui", "ChromeSavedDeskDelegate::GetFaviconForUrl");
  // Get the icons from lacros favicon service.
  if (crosapi::browser_util::IsLacrosEnabled()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->desk_template_ash()
        ->GetFaviconImage(GURL(page_url), lacros_profile_id,
                          std::move(callback));
    return;
  }

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile(),
          ServiceAccessType::EXPLICIT_ACCESS);

  favicon_service->GetRawFaviconForPageURL(
      GURL(page_url), {favicon_base::IconType::kFavicon}, 0,
      /*fallback_to_host=*/false,
      base::BindOnce(&ImageResultToImageSkia, std::move(callback)), tracker);
}

void ChromeSavedDeskDelegate::GetIconForAppId(
    const std::string& app_id,
    int desired_icon_size,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) const {
  TRACE_EVENT0("ui", "ChromeSavedDeskDelegate::GetIconForAppId");
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  if (!app_service_proxy) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  app_service_proxy->LoadIcon(app_id, apps::IconType::kStandard,
                              desired_icon_size,
                              /*allow_placeholder_icon=*/false,
                              AppIconResultToImageSkia(std::move(callback)));
}

bool ChromeSavedDeskDelegate::IsAppAvailable(const std::string& app_id) const {
  Profile* app_profile = ProfileManager::GetActiveUserProfile();
  DCHECK(app_profile);

  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(app_profile);
  CHECK(app_service_proxy);

  return ::IsAppAvailable(*app_service_proxy, app_id);
}

void ChromeSavedDeskDelegate::LaunchAppsFromSavedDesk(
    std::unique_ptr<ash::DeskTemplate> saved_desk) {
  std::vector<std::u16string> unavailable_apps =
      GetUnavailableAppNames(*saved_desk);
  // Show app unavailable toast.
  if (!unavailable_apps.empty()) {
    ShowUnavailableAppToast(unavailable_apps);
  }
  DesksClient::Get()->LaunchAppsFromTemplate(std::move(saved_desk));
}

// Returns true if `window` is supported in desk templates feature.
bool ChromeSavedDeskDelegate::IsWindowSupportedForSavedDesk(
    aura::Window* window) const {
  if (!window) {
    return false;
  }

  const auto* app_id = window->GetProperty(ash::kAppIDKey);
  // Feedback app is not saved, see http://b/301479278.
  if (app_id && *app_id == web_app::kOsFeedbackAppId) {
    return false;
  }

  if (!ash::DeskTemplate::IsAppTypeSupported(window)) {
    return false;
  }

  // Exclude incognito browser window.
  return IsWindowPersistable(window);
}

std::string ChromeSavedDeskDelegate::GetAppShortName(
    const std::string& app_id) {
  TRACE_EVENT0("ui", "ChromeSavedDeskDelegate::GetAppShortName");
  std::string name;
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* app_service_proxy =
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)
          ? apps::AppServiceProxyFactory::GetForProfile(profile)
          : nullptr;
  if (!app_service_proxy) {
    return name;
  }

  app_service_proxy->AppRegistryCache().ForOneApp(
      app_id,
      [&name](const apps::AppUpdate& update) { name = update.ShortName(); });
  return name;
}

void ChromeSavedDeskDelegate::OnLacrosChromeInfoReturned(
    GetAppLaunchDataCallback callback,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info,
    crosapi::mojom::DeskTemplateStatePtr state) {
  TRACE_EVENT0("ui", "ChromeSavedDeskDelegate::OnLacrosChromeInfoReturned");
  if (state.is_null()) {
    std::move(callback).Run({});
    return;
  }

  app_launch_info->browser_extra_info.urls = state->urls;
  app_launch_info->browser_extra_info.active_tab_index = state->active_index;
  app_launch_info->browser_extra_info.first_non_pinned_tab_index =
      state->first_non_pinned_index;
  if (state->browser_app_name.has_value()) {
    app_launch_info->browser_extra_info.app_type_browser = true;
    app_launch_info->browser_extra_info.app_name =
        state->browser_app_name.value();
  }
  app_launch_info->browser_extra_info.tab_group_infos =
      state->groups.value_or(std::vector<tab_groups::TabGroupInfo>());
  if (state->lacros_profile_id != 0) {
    app_launch_info->browser_extra_info.lacros_profile_id =
        state->lacros_profile_id;
  }

  std::move(callback).Run(std::move(app_launch_info));
}

void ChromeSavedDeskDelegate::GetLacrosChromeInfo(
    GetAppLaunchDataCallback callback,
    const std::string& window_unique_id,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info) const {
  TRACE_EVENT0("ui", "ChromeSavedDeskDelegate::GetLacrosChromeInfo");
  crosapi::BrowserManager* browser_manager = crosapi::BrowserManager::Get();
  if (!browser_manager || !browser_manager->IsRunning()) {
    LOG(WARNING)
        << "The browser manager is not running.  Cannot request browser state.";
    std::move(callback).Run({});
    return;
  }

  browser_manager->GetBrowserInformation(
      window_unique_id,
      base::BindOnce(&ChromeSavedDeskDelegate::OnLacrosChromeInfoReturned,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(app_launch_info)));
}
