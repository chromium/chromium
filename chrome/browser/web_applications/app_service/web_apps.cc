// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/intent_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"  // nogncheck
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_web_apps_utils.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/mall/mall_url.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#endif

using apps::IconEffects;

namespace web_app {

WebApps::WebApps(apps::AppServiceProxy* proxy)
    : apps::AppPublisher(proxy),
      profile_(proxy->profile()),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile_)),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      instance_registry_(&proxy->InstanceRegistry()),
#endif
      publisher_helper_(profile_, provider_, this) {
  Initialize();
}

WebApps::~WebApps() = default;

void WebApps::Shutdown() {
  if (provider_) {
    publisher_helper().Shutdown();
  }
}

const WebApp* WebApps::GetWebApp(const webapps::AppId& app_id) const {
  DCHECK(provider_);
  return provider_->registrar_unsafe().GetAppById(app_id);
}

void WebApps::Initialize() {
  DCHECK(profile_);

  // In some tests, WebAppPublisherHelper could be created during the shutdown
  // stage as the web app publisher is created async by AppServiceProxy. So
  // provider_ could be null in some tests.
  if (!AreWebAppsEnabled(profile_) || !provider_) {
    return;
  }

  provider_->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&WebApps::InitWebApps, weak_ptr_factory_.GetWeakPtr()));
}

void WebApps::LoadIcon(const std::string& app_id,
                       const apps::IconKey& icon_key,
                       apps::IconType icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       apps::LoadIconCallback callback) {
  publisher_helper().LoadIcon(app_id, icon_type, size_hint_in_dip,
                              static_cast<IconEffects>(icon_key.icon_effects),
                              std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApps::GetCompressedIconData(const std::string& app_id,
                                    int32_t size_in_dip,
                                    ui::ResourceScaleFactor scale_factor,
                                    apps::LoadIconCallback callback) {
  publisher_helper().GetCompressedIconData(app_id, size_in_dip, scale_factor,
                                           std::move(callback));
}
#endif

void WebApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::LaunchSource launch_source,
                     apps::WindowInfoPtr window_info) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Redirect launches of the Mall app so that we can add additional context to
  // the URL. Loading the context will cause a slight delay on first launch, but
  // it is then cached in the DeviceInfoManager for subsequent launches.
  // TODO(b/331702863): Remove this custom integration.
  if (chromeos::features::IsCrosMallWebAppEnabled() && app_id == kMallAppId) {
    apps::DeviceInfoManager* device_info_manager =
        apps::DeviceInfoManagerFactory::GetForProfile(profile());
    CHECK(device_info_manager);
    device_info_manager->GetDeviceInfo(base::BindOnce(
        &WebApps::LaunchMallWithContext, weak_ptr_factory_.GetWeakPtr(),
        event_flags, launch_source, std::move(window_info)));

    return;
  }
#endif

  publisher_helper().Launch(app_id, event_flags, launch_source,
                            std::move(window_info), base::DoNothing());
}

void WebApps::LaunchAppWithFiles(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::LaunchSource launch_source,
                                 std::vector<base::FilePath> file_paths) {
  publisher_helper().LaunchAppWithFiles(app_id, event_flags, launch_source,
                                        std::move(file_paths));
}

void WebApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  apps::IntentPtr intent,
                                  apps::LaunchSource launch_source,
                                  apps::WindowInfoPtr window_info,
                                  apps::LaunchCallback callback) {
  publisher_helper().LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                         launch_source, std::move(window_info),
                                         std::move(callback));
}

void WebApps::LaunchAppWithParams(apps::AppLaunchParams&& params,
                                  apps::LaunchCallback callback) {
  publisher_helper().LaunchAppWithParams(
      std::move(params),
      base::BindOnce(
          [](apps::LaunchCallback callback,
             content::WebContents* web_contents) {
            apps::LaunchResult::State result =
                web_contents ? apps::LaunchResult::State::kSuccess
                             : apps::LaunchResult::State::kFailed;
            std::move(callback).Run(apps::LaunchResult(result));
          },
          std::move(callback)));
}

void WebApps::LaunchShortcut(const std::string& app_id,
                             const std::string& shortcut_id,
                             int64_t display_id) {
  publisher_helper().ExecuteContextMenuCommand(app_id, shortcut_id, display_id,
                                               base::DoNothing());
}

void WebApps::SetPermission(const std::string& app_id,
                            apps::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

void WebApps::Uninstall(const std::string& app_id,
                        apps::UninstallSource uninstall_source,
                        bool clear_site_data,
                        bool report_abuse) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  publisher_helper().UninstallWebApp(web_app, uninstall_source, clear_site_data,
                                     report_abuse);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApps::GetMenuModel(const std::string& app_id,
                           apps::MenuType menu_type,
                           int64_t display_id,
                           base::OnceCallback<void(apps::MenuItems)> callback) {
  const auto* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::MenuItems());
    return;
  }

  bool can_close = true;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&can_close](const apps::AppUpdate& update) {
        can_close = update.AllowClose().value_or(true);
      });

  apps::MenuItems menu_items;
  auto* swa_manager = ash::SystemWebAppManager::Get(profile());
  if (swa_manager && swa_manager->IsSystemWebApp(web_app->app_id())) {
    DCHECK(web_app->client_data().system_web_app_data.has_value());
    ash::SystemWebAppType swa_type =
        web_app->client_data().system_web_app_data->system_app_type;

    auto* system_app = swa_manager->GetSystemApp(swa_type);
    if (system_app && system_app->ShouldShowNewWindowMenuOption()) {
      apps::AddCommandItem(ash::LAUNCH_NEW,
                           IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, menu_items);
    }
    // If app cannot be closed there should be no more than 1 open window, so we
    // should not allow open more windows because user won't be able to close
    // them.
  } else if (can_close) {
    // Isolated web apps can only be launched in new window.
    if (web_app->isolation_data().has_value()) {
      apps::AddCommandItem(ash::LAUNCH_NEW,
                           IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, menu_items);
    } else {
      apps::CreateOpenNewSubmenu(
          publisher_helper().GetWindowMode(app_id) == apps::WindowMode::kBrowser
              ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
              : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
          menu_items);
    }
  }

  if (app_id == guest_os::kTerminalSystemAppId) {
    guest_os::AddTerminalMenuItems(profile_, menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                         menu_items);
  }

  if (web_app->CanUserUninstallWebApp()) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         menu_items);
  }

  if (!web_app->IsSystemApp()) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         menu_items);
  }

  if (app_id == guest_os::kTerminalSystemAppId) {
    guest_os::AddTerminalMenuShortcuts(profile_, ash::LAUNCH_APP_SHORTCUT_FIRST,
                                       std::move(menu_items),
                                       std::move(callback));
  } else {
    GetAppShortcutMenuModel(app_id, std::move(menu_items), std::move(callback));
  }
}
#endif

void WebApps::UpdateAppSize(const std::string& app_id) {
  publisher_helper().UpdateAppSize(app_id);
}

void WebApps::SetWindowMode(const std::string& app_id,
                            apps::WindowMode window_mode) {
  publisher_helper().SetWindowMode(app_id, window_mode);
}

void WebApps::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void WebApps::PublishWebApps(std::vector<apps::AppPtr> apps) {
  if (!is_ready_) {
    return;
  }

  if (apps.empty()) {
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This is for prototyping and testing only. It is to provide an easy way to
  // simulate web app promise icon behaviour for the UI/ client development of
  // web app promise icons.
  // TODO(b/261907269): Remove this code snippet and use real listeners for web
  // app installation events.
  if (ash::features::ArePromiseIconsForWebAppsEnabled()) {
    for (auto& app : apps) {
      apps::MaybeSimulatePromiseAppInstallationEvents(proxy(), app.get());
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  apps::AppPublisher::Publish(std::move(apps), app_type(),
                              /*should_notify_initialized=*/false);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const WebApp* web_app = GetWebApp(ash::kChromeUIUntrustedProjectorSwaAppId);
  if (web_app) {
    proxy()->SetSupportedLinksPreference(
        ash::kChromeUIUntrustedProjectorSwaAppId);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void WebApps::PublishWebApp(apps::AppPtr app) {
  if (!is_ready_) {
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_projector = app->app_id == ash::kChromeUIUntrustedProjectorSwaAppId;

  // This is for prototyping and testing only.
  // TODO(b/261907269): Remove this code snippet and use real listeners for web
  // app installation events.
  if (ash::features::ArePromiseIconsForWebAppsEnabled()) {
    apps::MaybeSimulatePromiseAppInstallationEvents(proxy(), app.get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  apps::AppPublisher::Publish(std::move(app));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_projector) {
    // After OOBE, PublishWebApps() above could execute before the Projector app
    // has been registered. Since we need to call SetSupportedLinksPreference()
    // after the intent filter has been registered, we need this call for the
    // OOBE case.
    proxy()->SetSupportedLinksPreference(
        ash::kChromeUIUntrustedProjectorSwaAppId);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void WebApps::ModifyWebAppCapabilityAccess(
    const std::string& app_id,
    std::optional<bool> accessing_camera,
    std::optional<bool> accessing_microphone) {
  apps::AppPublisher::ModifyCapabilityAccess(
      app_id, std::move(accessing_camera), std::move(accessing_microphone));
}

std::vector<apps::AppPtr> WebApps::CreateWebApps() {
  DCHECK(provider_);

  std::vector<apps::AppPtr> apps;
  for (const WebApp& web_app : provider_->registrar_unsafe().GetApps()) {
    apps.push_back(publisher_helper().CreateWebApp(&web_app));
  }
  return apps;
}

void WebApps::InitWebApps() {
  TRACE_EVENT0("ui", "WebApps::InitWebApps");
  is_ready_ = true;

  RegisterPublisher(app_type());

  std::vector<apps::AppPtr> apps = CreateWebApps();

  apps::AppPublisher::Publish(std::move(apps), app_type(),
                              /*should_notify_initialized=*/true);
}


#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApps::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebApps::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebApps::StopApp(const std::string& app_id) {
  publisher_helper().StopApp(app_id);
}

void WebApps::GetAppShortcutMenuModel(
    const std::string& app_id,
    apps::MenuItems menu_items,
    base::OnceCallback<void(apps::MenuItems)> callback) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::MenuItems());
    return;
  }

  // Read shortcuts menu item icons from disk, if any.
  if (!web_app->shortcuts_menu_item_infos().empty()) {
    provider()->icon_manager().ReadAllShortcutsMenuIcons(
        app_id, base::BindOnce(&WebApps::OnShortcutsMenuIconsRead,
                               weak_ptr_factory_.GetWeakPtr(), app_id,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebApps::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    apps::MenuItems menu_items,
    base::OnceCallback<void(apps::MenuItems)> callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::MenuItems());
    return;
  }

  apps::AddSeparator(ui::DOUBLE_SEPARATOR, menu_items);

  size_t menu_item_index = 0;

  for (const WebAppShortcutsMenuItemInfo& menu_item_info :
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
      apps::AddSeparator(ui::PADDED_SEPARATOR, menu_items);
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
                                 menu_items);

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebApps::ExecuteContextMenuCommand(const std::string& app_id,
                                        int command_id,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  if (app_id == guest_os::kTerminalSystemAppId) {
    if (guest_os::ExecuteTerminalMenuShortcutCommand(profile_, shortcut_id,
                                                     display_id)) {
      return;
    }
  }
  publisher_helper().ExecuteContextMenuCommand(app_id, shortcut_id, display_id,
                                               base::DoNothing());
}

void WebApps::LaunchMallWithContext(int32_t event_flags,
                                    apps::LaunchSource launch_source,
                                    apps::WindowInfoPtr window_info,
                                    apps::DeviceInfo device_info) {
  LaunchAppWithIntent(
      kMallAppId, event_flags,
      std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                     ash::GetMallLaunchUrl(device_info)),
      launch_source, std::move(window_info), base::DoNothing());
}

#endif

}  // namespace web_app
