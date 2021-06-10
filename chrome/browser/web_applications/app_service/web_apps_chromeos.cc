// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps_chromeos.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/arc_web_contents_data.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_service_manager.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

using apps::IconEffects;

namespace web_app {

WebAppsChromeOs::BadgeManagerDelegate::BadgeManagerDelegate(
    const base::WeakPtr<WebAppsChromeOs>& web_apps_chrome_os)
    : badging::BadgeManagerDelegate(web_apps_chrome_os->profile(),
                                    web_apps_chrome_os->badge_manager_),
      web_apps_chrome_os_(web_apps_chrome_os) {}

WebAppsChromeOs::BadgeManagerDelegate::~BadgeManagerDelegate() = default;

void WebAppsChromeOs::BadgeManagerDelegate::OnAppBadgeUpdated(
    const AppId& app_id) {
  if (!web_apps_chrome_os_) {
    return;
  }
  apps::mojom::AppPtr app =
      web_apps_chrome_os_->app_notifications_.GetAppWithHasBadgeStatus(
          web_apps_chrome_os_->app_type(), app_id);
  app->has_badge = web_apps_chrome_os_->ShouldShowBadge(app_id, app->has_badge);
  web_apps_chrome_os_->Publish(std::move(app),
                               web_apps_chrome_os_->subscribers());
}

WebAppsChromeOs::WebAppsChromeOs(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile,
    apps::InstanceRegistry* instance_registry)
    : WebAppsBase(app_service, profile), instance_registry_(instance_registry) {
  DCHECK(instance_registry_);
  Initialize();
}

WebAppsChromeOs::~WebAppsChromeOs() {
  // In unit tests, AppServiceProxy might be ReInitializeForTesting, so
  // WebApps might be destroyed without calling Shutdown, so arc_prefs_
  // needs to be removed from observer in the destructor function.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

void WebAppsChromeOs::Shutdown() {
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }

  WebAppsBase::Shutdown();
}

void WebAppsChromeOs::ObserveArc() {
  // Observe the ARC apps to set the badge on the equivalent web app's icon.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
  }

  arc_prefs_ = ArcAppListPrefs::Get(profile());
  if (arc_prefs_) {
    arc_prefs_->AddObserver(this);
  }
}

void WebAppsChromeOs::Initialize() {
  DCHECK(profile());
  if (!AreWebAppsEnabled(profile())) {
    return;
  }

  media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance());

  notification_display_service_.Observe(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  badge_manager_ = badging::BadgeManagerFactory::GetForProfile(profile());
  // badge_manager_ is nullptr in guest and incognito profiles.
  if (badge_manager_) {
    badge_manager_->SetDelegate(
        std::make_unique<WebAppsChromeOs::BadgeManagerDelegate>(
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void WebAppsChromeOs::Uninstall(const std::string& app_id,
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

void WebAppsChromeOs::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebAppsChromeOs::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebAppsChromeOs::GetMenuModel(const std::string& app_id,
                                   apps::mojom::MenuType menu_type,
                                   int64_t display_id,
                                   GetMenuModelCallback callback) {
  bool is_system_web_app = false;
  bool can_use_uninstall = true;
  apps::mojom::WindowMode display_mode;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_system_web_app, &can_use_uninstall,
                          &display_mode](const apps::AppUpdate& update) {
        if (update.InstallSource() == apps::mojom::InstallSource::kSystem) {
          is_system_web_app = true;
        }
        if (update.InstallSource() == apps::mojom::InstallSource::kSystem ||
            update.InstallSource() == apps::mojom::InstallSource::kPolicy) {
          can_use_uninstall = false;
        }
        display_mode = update.WindowMode();
      });

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (!is_system_web_app) {
    apps::CreateOpenNewSubmenu(menu_type,
                               display_mode == apps::mojom::WindowMode::kBrowser
                                   ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
                                   : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
                               &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      !instance_registry_->GetWindows(app_id).empty()) {
    apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                         &menu_items);
  }

  if (can_use_uninstall) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         &menu_items);
  }

  if (!is_system_web_app) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         &menu_items);
  }

  GetMenuModelFromWebAppProvider(app_id, menu_type, std::move(menu_items),
                                 std::move(callback));
}

void WebAppsChromeOs::GetMenuModelFromWebAppProvider(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
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
        app_id,
        base::BindOnce(&WebAppsChromeOs::OnShortcutsMenuIconsRead,
                       base::AsWeakPtr<WebAppsChromeOs>(this), app_id,
                       menu_type, std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebAppsChromeOs::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
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
      if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
        // We apply masking to each shortcut icon, regardless if the purpose is
        // |MASKABLE| or |ANY|.
        icon_effects = apps::kCrOsStandardBackground | apps::kCrOsStandardMask;
      }

      icon = ConvertSquareBitmapsToImageSkia(
          *menu_item_icon_bitmaps, icon_effects,
          /*size_hint_in_dip=*/apps::kAppShortcutIconSizeDip);
    }

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + menu_item_index;
    // Passes menu_type argument as shortcut_id to use it in
    // ExecuteContextMenuCommand().
    std::string shortcut_id{apps::MenuTypeToString(menu_type)};

    const std::string label = base::UTF16ToUTF8(menu_item_info.name);

    apps::AddShortcutCommandItem(command_id, shortcut_id, label, icon,
                                 &menu_items);

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebAppsChromeOs::ExecuteContextMenuCommand(const std::string& app_id,
                                                int command_id,
                                                const std::string& shortcut_id,
                                                int64_t display_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  apps::mojom::LaunchSource launch_source;
  // shortcut_id contains menu_type.
  switch (apps::MenuTypeFromString(shortcut_id)) {
    case apps::mojom::MenuType::kShelf:
      launch_source = apps::mojom::LaunchSource::kFromShelf;
      break;
    case apps::mojom::MenuType::kAppList:
      launch_source = apps::mojom::LaunchSource::kFromAppListGridContextMenu;
      break;
  }

  DisplayMode display_mode = GetRegistrar()->GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params(
      app_id, ConvertDisplayModeToAppLaunchContainer(display_mode),
      WindowOpenDisposition::CURRENT_TAB,
      apps::GetAppLaunchSource(launch_source), display_id);

  size_t menu_item_index = command_id - ash::LAUNCH_APP_SHORTCUT_FIRST;
  if (menu_item_index < web_app->shortcuts_menu_item_infos().size()) {
    params.override_url =
        web_app->shortcuts_menu_item_infos()[menu_item_index].url;
  }

  publisher_helper().LaunchAppWithParams(std::move(params));
}

void WebAppsChromeOs::SetWindowMode(const std::string& app_id,
                                    apps::mojom::WindowMode window_mode) {
  publisher_helper().SetWindowMode(app_id, window_mode);
}

void WebAppsChromeOs::OnWebAppInstalled(const AppId& app_id) {
  provider()->registry_controller().SetAppIsDisabled(
      app_id, publisher_helper().IsWebAppInDisabledList(app_id));
  WebAppsBase::OnWebAppInstalled(app_id);
}

void WebAppsChromeOs::OnWebAppWillBeUninstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  app_notifications_.RemoveNotificationsForApp(app_id);
  publisher_helper().MaybeRemovePausedApp(app_id);

  auto result = media_requests_.RemoveRequests(app_id);
  ModifyCapabilityAccess(subscribers(), app_id, result.camera,
                         result.microphone);

  WebAppsBase::OnWebAppWillBeUninstalled(app_id);
}

void WebAppsChromeOs::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ApplyChromeBadge(package_info.package_name);
}

void WebAppsChromeOs::OnPackageRemoved(const std::string& package_name,
                                       bool uninstalled) {
  ApplyChromeBadge(package_name);
}

void WebAppsChromeOs::OnPackageListInitialRefreshed() {
  if (!arc_prefs_) {
    return;
  }

  for (const auto& app_name : arc_prefs_->GetPackagesFromPrefs()) {
    ApplyChromeBadge(app_name);
  }
}

void WebAppsChromeOs::OnArcAppListPrefsDestroyed() {
  arc_prefs_ = nullptr;
}

void WebAppsChromeOs::OnRequestUpdate(int render_process_id,
                                      int render_frame_id,
                                      blink::mojom::MediaStreamType stream_type,
                                      const content::MediaRequestState state) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));

  if (!web_contents) {
    return;
  }

  Profile* web_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (web_profile != profile()) {
    return;
  }

  absl::optional<AppId> app_id =
      FindInstalledAppWithUrlInScope(profile(), web_contents->GetURL(),
                                     /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app || !Accepts(app_id.value())) {
    return;
  }

  if (media_requests_.IsNewRequest(app_id.value(), web_contents, state)) {
    content::WebContentsUserData<
        apps::AppWebContentsData>::CreateForWebContents(web_contents, this);
  }

  auto result = media_requests_.UpdateRequests(app_id.value(), web_contents,
                                               stream_type, state);
  ModifyCapabilityAccess(subscribers(), app_id.value(), result.camera,
                         result.microphone);
}

void WebAppsChromeOs::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  absl::optional<AppId> app_id = FindInstalledAppWithUrlInScope(
      profile(), web_contents->GetLastCommittedURL(),
      /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app || !Accepts(app_id.value())) {
    return;
  }

  auto result =
      media_requests_.OnWebContentsDestroyed(app_id.value(), web_contents);
  ModifyCapabilityAccess(subscribers(), app_id.value(), result.camera,
                         result.microphone);
}

void WebAppsChromeOs::OnNotificationDisplayed(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  if (notification.notifier_id().type !=
      message_center::NotifierType::WEB_PAGE) {
    return;
  }
  MaybeAddWebPageNotifications(notification, metadata);
}

void WebAppsChromeOs::OnNotificationClosed(const std::string& notification_id) {
  auto app_ids = app_notifications_.GetAppIdsForNotification(notification_id);
  if (app_ids.empty()) {
    return;
  }

  app_notifications_.RemoveNotification(notification_id);

  for (const auto& app_id : app_ids) {
    apps::mojom::AppPtr app =
        app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id);
    app->has_badge = ShouldShowBadge(app_id, app->has_badge);
    Publish(std::move(app), subscribers());
  }
}

void WebAppsChromeOs::OnNotificationDisplayServiceDestroyed(
    NotificationDisplayService* service) {
  DCHECK(notification_display_service_.IsObservingSource(service));
  notification_display_service_.Reset();
}

bool WebAppsChromeOs::MaybeAddNotification(const std::string& app_id,
                                           const std::string& notification_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return false;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  apps::mojom::AppPtr app =
      app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id);
  app->has_badge = ShouldShowBadge(app_id, app->has_badge);
  Publish(std::move(app), subscribers());
  return true;
}

void WebAppsChromeOs::MaybeAddWebPageNotifications(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  const PersistentNotificationMetadata* persistent_metadata =
      PersistentNotificationMetadata::From(metadata);

  const NonPersistentNotificationMetadata* non_persistent_metadata =
      NonPersistentNotificationMetadata::From(metadata);

  if (persistent_metadata) {
    // For persistent notifications, find the web app with the SW scope url.
    absl::optional<AppId> app_id = FindInstalledAppWithUrlInScope(
        profile(), persistent_metadata->service_worker_scope,
        /*window_only=*/false);
    if (app_id.has_value()) {
      MaybeAddNotification(app_id.value(), notification.id());
    }
  } else {
    // For non-persistent notifications, find all web apps that are installed
    // under the origin url.
    DCHECK(provider());

    const GURL& url = non_persistent_metadata &&
                              !non_persistent_metadata->document_url.is_empty()
                          ? non_persistent_metadata->document_url
                          : notification.origin_url();

    auto app_ids = provider()->registrar().FindAppsInScope(url);
    int count = 0;
    for (const auto& app_id : app_ids) {
      if (MaybeAddNotification(app_id, notification.id())) {
        ++count;
      }
    }
    apps::RecordAppsPerNotification(count);
  }
}

apps::mojom::AppPtr WebAppsChromeOs::Convert(const WebApp* web_app,
                                             apps::mojom::Readiness readiness) {
  DCHECK(web_app->chromeos_data().has_value());
  bool is_disabled = web_app->chromeos_data()->is_disabled;
  apps::mojom::AppPtr app = publisher_helper().ConvertWebApp(
      web_app,
      is_disabled ? apps::mojom::Readiness::kDisabledByPolicy : readiness);

  bool paused = publisher_helper().IsPaused(web_app->app_id());
  app->icon_key = publisher_helper().MakeIconKey(web_app);

  apps::mojom::OptionalBool has_notification =
      app_notifications_.HasNotification(web_app->app_id())
          ? apps::mojom::OptionalBool::kTrue
          : apps::mojom::OptionalBool::kFalse;
  app->has_badge = ShouldShowBadge(web_app->app_id(), has_notification);
  app->paused = paused ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
  return app;
}

void WebAppsChromeOs::ApplyChromeBadge(const std::string& package_name) {
  const std::vector<std::string> app_ids =
      extensions::util::GetEquivalentInstalledAppIds(package_name);

  for (auto& app_id : app_ids) {
    if (GetWebApp(app_id)) {
      publisher_helper().SetIconEffect(app_id);
    }
  }
}

apps::mojom::OptionalBool WebAppsChromeOs::ShouldShowBadge(
    const std::string& app_id,
    apps::mojom::OptionalBool has_notification) {
  bool enabled =
      base::FeatureList::IsEnabled(features::kDesktopPWAsAttentionBadgingCrOS);
  std::string flag =
      enabled ? features::kDesktopPWAsAttentionBadgingCrOSParam.Get() : "";
  if (flag == switches::kDesktopPWAsAttentionBadgingCrOSApiOnly) {
    // Show a badge based only on the Web Badging API.
    return badge_manager_ && badge_manager_->GetBadgeValue(app_id).has_value()
               ? apps::mojom::OptionalBool::kTrue
               : apps::mojom::OptionalBool::kFalse;
  } else if (flag ==
             switches::kDesktopPWAsAttentionBadgingCrOSApiAndNotifications) {
    // When the flag is set to "api-and-notifications" we show a badge if either
    // a notification is showing or the Web Badging API has a badge set.
    return badge_manager_ && badge_manager_->GetBadgeValue(app_id).has_value()
               ? apps::mojom::OptionalBool::kTrue
               : has_notification;
  } else if (flag ==
             switches::
                 kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications) {
    // When the flag is set to "api-overrides-notifications" we show a badge if
    // either the Web Badging API recently has a badge set, or the Badging API
    // has not been recently used by the app and a notification is showing.
    if (!badge_manager_ || !badge_manager_->HasRecentApiUsage(app_id))
      return has_notification;

    return badge_manager_->GetBadgeValue(app_id).has_value()
               ? apps::mojom::OptionalBool::kTrue
               : apps::mojom::OptionalBool::kFalse;
  } else {
    // Show a badge only if a notification is showing.
    return has_notification;
  }
}

}  // namespace web_app
