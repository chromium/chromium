// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/projector_app/projector_app_constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#endif

using apps::IconEffects;

namespace web_app {

namespace {

apps::mojom::AppType GetWebAppType() {
// After moving the ordinary Web Apps to Lacros chrome, the remaining web
// apps in ash Chrome will be only System Web Apps. Change the app type
// to kSystemWeb for this case and the kWeb app type will be published from
// the publisher for Lacros web apps.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled() &&
      base::FeatureList::IsEnabled(features::kWebAppsCrosapi)) {
    return apps::mojom::AppType::kSystemWeb;
  }
#endif

  return apps::mojom::AppType::kWeb;
}

bool ShouldObserveMediaRequests() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The publisher helper owned by WebAppsPublisherHost observes media requests,
  // not the publisher helper owned by WebApps.
  return false;
#else
  return true;
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Make |app_id| the preferred app for handling |url|, without needing the user
// to choose the app through an intent picker. This function must be called
// after the corresponding intent filter has already been registered.
void AddDefaultPreferredApp(const std::string& app_id,
                            const GURL& url,
                            apps::mojom::AppService* app_service) {
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(url);
  app_service->AddPreferredApp(GetWebAppType(), app_id,
                               std::move(intent_filter),
                               /*intent=*/nullptr, /*from_publisher=*/true);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

WebApps::WebApps(const mojo::Remote<apps::mojom::AppService>& app_service,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                 apps::InstanceRegistry* instance_registry,
#endif
                 Profile* profile)
    : profile_(profile),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile_)),
      app_service_(nullptr),
      app_type_(GetWebAppType()),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      instance_registry_(instance_registry),
#endif
      publisher_helper_(profile_,
                        provider_,
                        app_type_,
                        this,
                        ShouldObserveMediaRequests()) {
  Initialize(app_service);
}

WebApps::~WebApps() = default;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// static
void WebApps::UninstallImpl(WebAppProvider* provider,
                            const std::string& app_id,
                            apps::mojom::UninstallSource uninstall_source,
                            gfx::NativeWindow parent_window) {
  WebAppUiManagerImpl* web_app_ui_manager = WebAppUiManagerImpl::Get(provider);
  if (!web_app_ui_manager) {
    return;
  }

  WebAppDialogManager& web_app_dialog_manager =
      web_app_ui_manager->dialog_manager();
  if (web_app_dialog_manager.CanUserUninstallWebApp(app_id)) {
    webapps::WebappUninstallSource webapp_uninstall_source =
        WebAppPublisherHelper::ConvertUninstallSourceToWebAppUninstallSource(
            uninstall_source);
    web_app_dialog_manager.UninstallWebApp(app_id, webapp_uninstall_source,
                                           parent_window, base::DoNothing());
  }
}
#endif

void WebApps::Shutdown() {
  if (provider_) {
    publisher_helper().Shutdown();
  }
}

const WebApp* WebApps::GetWebApp(const AppId& app_id) const {
  DCHECK(provider_);
  return provider_->registrar().GetAppById(app_id);
}

bool WebApps::Accepts(const std::string& app_id) const {
  return WebAppPublisherHelper::Accepts(app_id);
}

void WebApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  DCHECK(provider_);

  PublisherBase::Initialize(app_service, app_type_);
  app_service_ = app_service.get();
}

void WebApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebApps::StartPublishingWebApps, AsWeakPtr(),
                                std::move(subscriber_remote)));
}

void WebApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconType icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  publisher_helper().LoadIcon(app_id, std::move(icon_key), std::move(icon_type),
                              size_hint_in_dip, allow_placeholder_icon,
                              std::move(callback));
}

void WebApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().Launch(app_id, event_flags, launch_source,
                            std::move(window_info));
}

void WebApps::LaunchAppWithFiles(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::FilePathsPtr file_paths) {
  publisher_helper().LaunchAppWithFiles(app_id, event_flags, launch_source,
                                        std::move(file_paths));
}

void WebApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  apps::mojom::IntentPtr intent,
                                  apps::mojom::LaunchSource launch_source,
                                  apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                         launch_source, std::move(window_info));
}

void WebApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

void WebApps::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void WebApps::PublishWebApps(std::vector<apps::mojom::AppPtr> apps) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const WebApp* web_app =
      GetWebApp(chromeos::kChromeUITrustedProjectorSwaAppId);
  if (web_app) {
    AddDefaultPreferredApp(chromeos::kChromeUITrustedProjectorSwaAppId,
                           GURL(chromeos::kChromeUIUntrustedProjectorPwaUrl),
                           app_service_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const bool should_notify_initialized = false;
  if (subscribers_.size() == 1) {
    auto& subscriber = *subscribers_.begin();
    subscriber->OnApps(std::move(apps), app_type(), should_notify_initialized);
    return;
  }
  for (auto& subscriber : subscribers_) {
    std::vector<apps::mojom::AppPtr> cloned_apps;
    for (const auto& app : apps)
      cloned_apps.push_back(app.Clone());
    subscriber->OnApps(std::move(cloned_apps), app_type(),
                       should_notify_initialized);
  }
}

void WebApps::PublishWebApp(apps::mojom::AppPtr app) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app->app_id == chromeos::kChromeUITrustedProjectorSwaAppId) {
    // After OOBE, PublishWebApps() above could execute before the intent filter
    // has been registered. Since we need to call AddDefaultPreferredApp() after
    // the intent filter has been registered, we need this call for the OOBE
    // case.
    AddDefaultPreferredApp(chromeos::kChromeUITrustedProjectorSwaAppId,
                           GURL(chromeos::kChromeUIUntrustedProjectorPwaUrl),
                           app_service_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  Publish(std::move(app), subscribers_);
}

void WebApps::ModifyWebAppCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  ModifyCapabilityAccess(subscribers_, app_id, std::move(accessing_camera),
                         std::move(accessing_microphone));
}

void WebApps::ConvertWebApps(std::vector<apps::mojom::AppPtr>* apps_out) {
  DCHECK(provider_);

  for (const WebApp& web_app : provider_->registrar().GetApps()) {
    if (Accepts(web_app.app_id())) {
      apps_out->push_back(publisher_helper().ConvertWebApp(&web_app));
    }
  }
}

void WebApps::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(&apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), app_type_,
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApps::Uninstall(const std::string& app_id,
                        apps::mojom::UninstallSource uninstall_source,
                        bool clear_site_data,
                        bool report_abuse) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  publisher_helper().UninstallWebApp(web_app, uninstall_source, clear_site_data,
                                     report_abuse);
}

void WebApps::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebApps::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebApps::StopApp(const std::string& app_id) {
  publisher_helper().StopApp(app_id);
}

void WebApps::GetMenuModel(const std::string& app_id,
                           apps::mojom::MenuType menu_type,
                           int64_t display_id,
                           GetMenuModelCallback callback) {
  const auto* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();
  if (web_app->IsSystemApp()) {
    DCHECK(web_app->client_data().system_web_app_data.has_value());
    SystemAppType swa_type =
        web_app->client_data().system_web_app_data->system_app_type;

    auto* system_app = WebAppProvider::GetForSystemWebApps(profile())
                           ->system_web_app_manager()
                           .GetSystemApp(swa_type);
    if (system_app && system_app->ShouldShowNewWindowMenuOption()) {
      apps::AddCommandItem(ash::MENU_OPEN_NEW,
                           IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, &menu_items);
    }
  } else {
    apps::CreateOpenNewSubmenu(menu_type,
                               publisher_helper().GetWindowMode(app_id) ==
                                       apps::mojom::WindowMode::kBrowser
                                   ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
                                   : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
                               &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      instance_registry_->ContainsAppId(app_id)) {
    apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                         &menu_items);
  }

  if (web_app->CanUserUninstallWebApp()) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         &menu_items);
  }

  if (!web_app->IsSystemApp()) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         &menu_items);
  }

  GetAppShortcutMenuModel(app_id, std::move(menu_items), std::move(callback));
}

void WebApps::GetAppShortcutMenuModel(const std::string& app_id,
                                      apps::mojom::MenuItemsPtr menu_items,
                                      GetMenuModelCallback callback) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  // Read shortcuts menu item icons from disk, if any.
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenuUI) &&
      !web_app->shortcuts_menu_item_infos().empty()) {
    provider()->icon_manager().ReadAllShortcutsMenuIcons(
        app_id, base::BindOnce(&WebApps::OnShortcutsMenuIconsRead,
                               base::AsWeakPtr<WebApps>(this), app_id,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebApps::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::AddSeparator(ui::DOUBLE_SEPARATOR, &menu_items);

  size_t menu_item_index = 0;

  for (const WebApplicationShortcutsMenuItemInfo& menu_item_info :
       web_app->shortcuts_menu_item_infos()) {
    const std::map<SquareSizePx, SkBitmap>* menu_item_icon_bitmaps = nullptr;
    if (menu_item_index < shortcuts_menu_icon_bitmaps.size()) {
      // We prefer |MASKABLE| icons, but fall back to icons with purpose |ANY|.
      menu_item_icon_bitmaps =
          &shortcuts_menu_icon_bitmaps[menu_item_index].maskable;
      if (menu_item_icon_bitmaps->empty()) {
        menu_item_icon_bitmaps =
            &shortcuts_menu_icon_bitmaps[menu_item_index].any;
      }
    }

    if (menu_item_index != 0) {
      apps::AddSeparator(ui::PADDED_SEPARATOR, &menu_items);
    }

    gfx::ImageSkia icon;
    if (menu_item_icon_bitmaps) {
      IconEffects icon_effects = IconEffects::kNone;

      // We apply masking to each shortcut icon, regardless if the purpose is
      // |MASKABLE| or |ANY|.
      icon_effects = apps::kCrOsStandardBackground | apps::kCrOsStandardMask;

      icon = ConvertSquareBitmapsToImageSkia(
          *menu_item_icon_bitmaps, icon_effects,
          /*size_hint_in_dip=*/apps::kAppShortcutIconSizeDip);
    }

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + menu_item_index;

    const std::string label = base::UTF16ToUTF8(menu_item_info.name);
    std::string shortcut_id = publisher_helper().GenerateShortcutId();
    publisher_helper().StoreShortcutId(shortcut_id, menu_item_info);

    apps::AddShortcutCommandItem(command_id, shortcut_id, label, icon,
                                 &menu_items);

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebApps::ExecuteContextMenuCommand(const std::string& app_id,
                                        int command_id,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  publisher_helper().ExecuteContextMenuCommand(app_id, shortcut_id, display_id);
}

void WebApps::SetWindowMode(const std::string& app_id,
                            apps::mojom::WindowMode window_mode) {
  publisher_helper().SetWindowMode(app_id, window_mode);
}
#endif

}  // namespace web_app
