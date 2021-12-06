// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/common/chrome_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/arc/arc_web_contents_data.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/sessions/core/session_id.h"
#endif

using apps::IconEffects;

namespace web_app {

namespace {

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

bool GetContentSettingsType(apps::mojom::PermissionType permission_type,
                            ContentSettingsType& content_setting_type) {
  switch (permission_type) {
    case apps::mojom::PermissionType::kCamera:
      content_setting_type = ContentSettingsType::MEDIASTREAM_CAMERA;
      return true;
    case apps::mojom::PermissionType::kLocation:
      content_setting_type = ContentSettingsType::GEOLOCATION;
      return true;
    case apps::mojom::PermissionType::kMicrophone:
      content_setting_type = ContentSettingsType::MEDIASTREAM_MIC;
      return true;
    case apps::mojom::PermissionType::kNotifications:
      content_setting_type = ContentSettingsType::NOTIFICATIONS;
      return true;
    case apps::mojom::PermissionType::kUnknown:
    case apps::mojom::PermissionType::kContacts:
    case apps::mojom::PermissionType::kStorage:
    case apps::mojom::PermissionType::kPrinting:
      return false;
  }
}

apps::mojom::PermissionType GetPermissionType(
    ContentSettingsType content_setting_type) {
  switch (content_setting_type) {
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return apps::mojom::PermissionType::kCamera;
    case ContentSettingsType::GEOLOCATION:
      return apps::mojom::PermissionType::kLocation;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return apps::mojom::PermissionType::kMicrophone;
    case ContentSettingsType::NOTIFICATIONS:
      return apps::mojom::PermissionType::kNotifications;
    default:
      return apps::mojom::PermissionType::kUnknown;
  }
}

apps::mojom::InstallReason GetHighestPriorityInstallReason(
    const WebApp* web_app) {
  // TODO(crbug.com/1189949): Introduce kOem as a new Source::Type value
  // immediately below web_app::Source::kSystem, so that this custom behavior
  // isn't needed.
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    if (chromeos_data.oem_installed) {
      DCHECK(!web_app->IsSystemApp());
      return apps::mojom::InstallReason::kOem;
    }
  }

  switch (web_app->GetHighestPrioritySource()) {
    case Source::kSystem:
      return apps::mojom::InstallReason::kSystem;
    case Source::kPolicy:
      return apps::mojom::InstallReason::kPolicy;
    case Source::kSubApp:
      return apps::mojom::InstallReason::kSubApp;
    case Source::kWebAppStore:
      return apps::mojom::InstallReason::kUser;
    case Source::kSync:
      return apps::mojom::InstallReason::kSync;
    case Source::kDefault:
      return apps::mojom::InstallReason::kDefault;
  }
}

apps::mojom::InstallSource GetInstallSource(PrefService* prefs,
                                            const AppId& app_id) {
  auto install_source = web_app::GetWebAppInstallSource(prefs, app_id);
  if (!install_source.has_value()) {
    return apps::mojom::InstallSource::kUnknown;
  }

  switch (static_cast<webapps::WebappInstallSource>(install_source.value())) {
    case webapps::WebappInstallSource::MENU_BROWSER_TAB:
    case webapps::WebappInstallSource::MENU_CUSTOM_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case webapps::WebappInstallSource::API_BROWSER_TAB:
    case webapps::WebappInstallSource::API_CUSTOM_TAB:
    case webapps::WebappInstallSource::DEVTOOLS:
    case webapps::WebappInstallSource::MANAGEMENT_API:
    case webapps::WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case webapps::WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case webapps::WebappInstallSource::EXTERNAL_POLICY:
    case webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case webapps::WebappInstallSource::MENU_CREATE_SHORTCUT:
    case webapps::WebappInstallSource::SUB_APP:
      return apps::mojom::InstallSource::kBrowser;
    case webapps::WebappInstallSource::ARC:
      return apps::mojom::InstallSource::kPlayStore;
    case webapps::WebappInstallSource::INTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_DEFAULT:
    case webapps::WebappInstallSource::SYSTEM_DEFAULT:
      return apps::mojom::InstallSource::kSystem;
    case webapps::WebappInstallSource::SYNC:
      return apps::mojom::InstallSource::kSync;
    case webapps::WebappInstallSource::COUNT:
      return apps::mojom::InstallSource::kUnknown;
  }
}

}  // namespace

WebAppPublisherHelper::Delegate::Delegate() = default;

WebAppPublisherHelper::Delegate::~Delegate() = default;

#if defined(OS_CHROMEOS)
WebAppPublisherHelper::BadgeManagerDelegate::BadgeManagerDelegate(
    const base::WeakPtr<WebAppPublisherHelper>& publisher_helper)
    : badging::BadgeManagerDelegate(publisher_helper->profile(),
                                    publisher_helper->badge_manager_),
      publisher_helper_(publisher_helper) {}

WebAppPublisherHelper::BadgeManagerDelegate::~BadgeManagerDelegate() = default;

void WebAppPublisherHelper::BadgeManagerDelegate::OnAppBadgeUpdated(
    const AppId& app_id) {
  if (!publisher_helper_) {
    return;
  }
  apps::mojom::AppPtr app =
      publisher_helper_->app_notifications_.GetAppWithHasBadgeStatus(
          publisher_helper_->app_type(), app_id);
  app->has_badge = publisher_helper_->ShouldShowBadge(app_id, app->has_badge);
  publisher_helper_->delegate_->PublishWebApp(std::move(app));
}
#endif

WebAppPublisherHelper::WebAppPublisherHelper(Profile* profile,
                                             WebAppProvider* provider,
                                             apps::mojom::AppType app_type,
                                             Delegate* delegate,
                                             bool observe_media_requests)
    : profile_(profile),
      provider_(provider),
      app_type_(app_type),
      delegate_(delegate) {
  DCHECK(profile_);
  Init(observe_media_requests);
}

WebAppPublisherHelper::~WebAppPublisherHelper() = default;

// static
bool WebAppPublisherHelper::IsSupportedWebAppPermissionType(
    ContentSettingsType permission_type) {
  return base::Contains(kSupportedPermissionTypes, permission_type);
}

// static
webapps::WebappUninstallSource
WebAppPublisherHelper::ConvertUninstallSourceToWebAppUninstallSource(
    apps::mojom::UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case apps::mojom::UninstallSource::kAppList:
      return webapps::WebappUninstallSource::kAppList;
    case apps::mojom::UninstallSource::kAppManagement:
      return webapps::WebappUninstallSource::kAppManagement;
    case apps::mojom::UninstallSource::kShelf:
      return webapps::WebappUninstallSource::kShelf;
    case apps::mojom::UninstallSource::kMigration:
      return webapps::WebappUninstallSource::kMigration;
    case apps::mojom::UninstallSource::kUnknown:
      return webapps::WebappUninstallSource::kUnknown;
  }
}

// static
bool WebAppPublisherHelper::Accepts(const std::string& app_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Crostini Terminal System App is handled by Crostini Apps.
  return app_id != crostini::kCrostiniTerminalSystemAppId;
#else
  return true;
#endif
}

void WebAppPublisherHelper::Shutdown() {
  registrar_observation_.Reset();
  content_settings_observation_.Reset();
}

void WebAppPublisherHelper::SetWebAppShowInFields(apps::mojom::AppPtr& app,
                                                  const WebApp* web_app) {
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    app->show_in_launcher = chromeos_data.show_in_launcher
                                ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = app->show_in_search =
        chromeos_data.show_in_search ? apps::mojom::OptionalBool::kTrue
                                     : apps::mojom::OptionalBool::kFalse;
    app->show_in_management = chromeos_data.show_in_management
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;
    return;
  }

  // Show the app everywhere by default.
  auto show = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->show_in_shelf = show;
  app->show_in_search = show;
  app->show_in_management = show;
}

void WebAppPublisherHelper::PopulateWebAppPermissions(
    const WebApp* web_app,
    std::vector<apps::mojom::PermissionPtr>* target) {
  const GURL url = web_app->start_url();

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting =
        host_content_settings_map->GetContentSetting(url, url, type);

    // Map ContentSettingsType to an apps::mojom::TriState value
    apps::mojom::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::mojom::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::mojom::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::mojom::TriState::kBlock;
        break;
      default:
        setting_val = apps::mojom::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, &setting_info);

    auto permission = apps::mojom::Permission::New();
    permission->permission_type = GetPermissionType(type);
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(setting_val);
    permission->is_managed =
        setting_info.source == content_settings::SETTING_SOURCE_POLICY;

    target->push_back(std::move(permission));
  }
}

apps::mojom::AppPtr WebAppPublisherHelper::ConvertWebApp(
    const WebApp* web_app) {
  apps::mojom::Readiness readiness =
      web_app->is_locally_installed() ? apps::mojom::Readiness::kReady
                                      : apps::mojom::Readiness::kDisabledByUser;
#if defined(OS_CHROMEOS)
  DCHECK(web_app->chromeos_data().has_value());
  if (web_app->chromeos_data()->is_disabled)
    readiness = apps::mojom::Readiness::kDisabledByPolicy;
#endif

  auto install_reason = GetHighestPriorityInstallReason(web_app);
  apps::mojom::AppPtr app =
      apps::PublisherBase::MakeApp(app_type(), web_app->app_id(), readiness,
                                   web_app->name(), install_reason);

  app->install_source =
      GetInstallSource(profile()->GetPrefs(), web_app->app_id());

  GURL install_url;
  if (registrar().HasExternalAppWithInstallSource(
          web_app->app_id(), ExternalInstallSource::kExternalPolicy)) {
    std::map<AppId, GURL> installed_apps =
        registrar().GetExternallyInstalledApps(
            ExternalInstallSource::kExternalPolicy);
    auto it = installed_apps.find(web_app->app_id());
    if (it != installed_apps.end()) {
      install_url = it->second;
    }
  }
  app->policy_id = install_url.spec();

  // For system web apps (only), the install source is |kSystem|.
  DCHECK_EQ(web_app->IsSystemApp(),
            app->install_reason == apps::mojom::InstallReason::kSystem);

  app->description = web_app->description();
  app->additional_search_terms = web_app->additional_search_terms();
  app->last_launch_time = web_app->last_launch_time();
  app->install_time = web_app->install_time();

  // Web App's publisher_id the start url.
  app->publisher_id = web_app->start_url().spec();

  auto display_mode = registrar().GetAppUserDisplayMode(web_app->app_id());
  app->window_mode = ConvertDisplayModeToWindowMode(display_mode);

  // app->version is left empty here.
  PopulateWebAppPermissions(web_app, &app->permissions);

  SetWebAppShowInFields(app, web_app);

  // Add the intent filters for PWAs.
  base::Extend(app->intent_filters,
               apps_util::CreateWebAppIntentFilters(
                   *web_app, registrar().GetAppScope(web_app->app_id())));

  app->icon_key = MakeIconKey(web_app);

  bool paused = IsPaused(web_app->app_id());
  app->paused = paused ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;

#if defined(OS_CHROMEOS)
  if (readiness != apps::mojom::Readiness::kReady) {
    UpdateAppDisabledMode(app);
  }

  apps::mojom::OptionalBool has_notification =
      app_notifications_.HasNotification(web_app->app_id())
          ? apps::mojom::OptionalBool::kTrue
          : apps::mojom::OptionalBool::kFalse;
  app->has_badge = ShouldShowBadge(web_app->app_id(), has_notification);
#else
  app->has_badge = apps::mojom::OptionalBool::kFalse;
#endif

  return app;
}

apps::mojom::AppPtr WebAppPublisherHelper::ConvertUninstalledWebApp(
    const WebApp* web_app) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type();
  app->app_id = web_app->app_id();
  // TODO(loyso): Plumb uninstall source (reason) here.
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;

  SetWebAppShowInFields(app, web_app);
  return app;
}

apps::mojom::AppPtr WebAppPublisherHelper::ConvertLaunchedWebApp(
    const WebApp* web_app) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type();
  app->app_id = web_app->app_id();
  app->last_launch_time = web_app->last_launch_time();
  return app;
}

void WebAppPublisherHelper::UninstallWebApp(
    const WebApp* web_app,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  auto origin = url::Origin::Create(web_app->start_url());

  DCHECK(provider_);
  DCHECK(
      provider_->install_finalizer().CanUserUninstallWebApp(web_app->app_id()));
  webapps::WebappUninstallSource webapp_uninstall_source =
      ConvertUninstallSourceToWebAppUninstallSource(uninstall_source);
  provider_->install_finalizer().UninstallWebApp(
      web_app->app_id(), webapp_uninstall_source, base::DoNothing());
  web_app = nullptr;

  if (!clear_site_data) {
    // TODO(crbug.com/1062885): Add UMA_HISTOGRAM_ENUMERATION here.
    return;
  }

  // TODO(crbug.com/1062885): Add UMA_HISTOGRAM_ENUMERATION here.
  constexpr bool kClearCookies = true;
  constexpr bool kClearStorage = true;
  constexpr bool kClearCache = true;
  constexpr bool kAvoidClosingConnections = false;

  content::ClearSiteData(base::BindRepeating(
                             [](content::BrowserContext* browser_context) {
                               return browser_context;
                             },
                             base::Unretained(profile())),
                         origin, kClearCookies, kClearStorage, kClearCache,
                         kAvoidClosingConnections, base::DoNothing());
}

apps::mojom::IconKeyPtr WebAppPublisherHelper::MakeIconKey(
    const WebApp* web_app) {
  return icon_key_factory_.MakeIconKey(GetIconEffects(web_app));
}

void WebAppPublisherHelper::SetIconEffect(const std::string& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type();
  app->app_id = app_id;
  app->icon_key = MakeIconKey(web_app);
  delegate_->PublishWebApp(std::move(app));
}

void WebAppPublisherHelper::PauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = true;
  delegate_->PublishWebApp(
      paused_apps_.GetAppWithPauseStatus(app_type(), app_id, kPaused));

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_app()) {
      continue;
    }
    if (GetAppIdFromApplicationName(browser->app_name()) == app_id) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }
}

void WebAppPublisherHelper::UnpauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = false;
  delegate_->PublishWebApp(
      paused_apps_.GetAppWithPauseStatus(app_type(), app_id, kPaused));
}

bool WebAppPublisherHelper::IsPaused(const std::string& app_id) {
  return paused_apps_.IsPaused(app_id);
}

void WebAppPublisherHelper::LoadIcon(const std::string& app_id,
                                     apps::mojom::IconKeyPtr icon_key,
                                     apps::mojom::IconType icon_type,
                                     int32_t size_hint_in_dip,
                                     bool allow_placeholder_icon,
                                     LoadIconCallback callback) {
  DCHECK(provider_);

  if (icon_key) {
    LoadIconFromWebApp(profile_, icon_type, size_hint_in_dip, app_id,
                       static_cast<IconEffects>(icon_key->icon_effects),
                       std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

content::WebContents* WebAppPublisherHelper::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  if (!profile_) {
    return nullptr;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return nullptr;
  }

  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunch",
                                extension_misc::APP_LAUNCH_APP_LIST_MAIN,
                                extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunch",
                                extension_misc::APP_LAUNCH_APP_LIST_SEARCH,
                                extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromKeyboard:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromInstalledNotification:
    case apps::mojom::LaunchSource::kFromTest:
    case apps::mojom::LaunchSource::kFromArc:
    case apps::mojom::LaunchSource::kFromSharesheet:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
      break;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      web_app->app_id(), event_flags, apps::GetAppLaunchSource(launch_source),
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));

  // The app will be launched for the currently active profile.
  return LaunchAppWithParams(std::move(params));
}

content::WebContents* WebAppPublisherHelper::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);
  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, apps::GetAppLaunchSource(launch_source),
      display::kInvalidDisplayId,
      /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));
  if (file_paths) {
    for (const auto& file_path : file_paths->file_paths) {
      params.launch_files.push_back(file_path);
    }
  }

  // The app will be launched for the currently active profile.
  return LaunchAppWithParams(std::move(params));
}

content::WebContents* WebAppPublisherHelper::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  CHECK(intent);
  content::WebContents* web_contents = LaunchAppWithIntentImpl(
      app_id, event_flags, std::move(intent), launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);

// TODO(crbug.com/1214763): Set ArcWebContentsData for Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (launch_source == apps::mojom::LaunchSource::kFromArc && web_contents) {
    // Add a flag to remember this tab originated in the ARC context.
    web_contents->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                              std::make_unique<arc::ArcWebContentsData>());
  }
#endif

  return web_contents;
}

content::WebContents* WebAppPublisherHelper::LaunchAppWithParams(
    apps::AppLaunchParams params) {
  apps::AppLaunchParams params_for_restore(
      params.app_id, params.container, params.disposition, params.source,
      params.display_id, params.launch_files, params.intent);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Create the FullRestoreSaveHandler instance before launching the app to
  // observe the browser window.
  full_restore::FullRestoreSaveHandler::GetInstance();
#endif

  content::WebContents* const web_contents =
      web_app_launch_manager_->OpenApplication(std::move(params));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Save all launch information for system web apps, because the browser
  // session restore can't restore system web apps.
  int session_id = apps::GetSessionIdForRestoreFromWebContents(web_contents);
  if (SessionID::IsValidValue(session_id)) {
    const WebApp* web_app = GetWebApp(params_for_restore.app_id);
    const bool is_system_web_app = web_app && web_app->IsSystemApp();
    if (is_system_web_app) {
      std::unique_ptr<app_restore::AppLaunchInfo> launch_info =
          std::make_unique<app_restore::AppLaunchInfo>(
              params_for_restore.app_id, session_id,
              params_for_restore.container, params_for_restore.disposition,
              params_for_restore.display_id,
              std::move(params_for_restore.launch_files),
              std::move(params_for_restore.intent));
      full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                      std::move(launch_info));
    }
  }
#endif

  return web_contents;
}

void WebAppPublisherHelper::SetPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  if (!profile_) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = web_app->start_url();

  ContentSettingsType permission_type;

  if (!GetContentSettingsType(permission->permission_type, permission_type)) {
    return;
  }

  DCHECK_EQ(permission->value_type,
            apps::mojom::PermissionValueType::kTriState);
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (static_cast<apps::mojom::TriState>(permission->value)) {
    case apps::mojom::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::mojom::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::mojom::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, permission_value);
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
void WebAppPublisherHelper::StopApp(const std::string& app_id) {
  if (!features::IsBrowserAppInstanceTrackingEnabled()) {
    return;
  }

  apps::BrowserAppInstanceTracker* instance_tracker =
      apps::AppServiceProxyFactory::GetInstance()
          ->GetForProfile(profile_)
          ->BrowserAppInstanceTracker();

  instance_tracker->StopInstancesOfApp(app_id);
}
#endif

void WebAppPublisherHelper::OpenNativeSettings(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  chrome::ShowSiteSettings(profile(), web_app->start_url());
}

apps::mojom::WindowMode WebAppPublisherHelper::GetWindowMode(
    const std::string& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app)
    return apps::mojom::WindowMode::kUnknown;

  auto display_mode = registrar().GetAppUserDisplayMode(web_app->app_id());
  return ConvertDisplayModeToWindowMode(display_mode);
}

void WebAppPublisherHelper::SetWindowMode(const std::string& app_id,
                                          apps::mojom::WindowMode window_mode) {
  auto display_mode = blink::mojom::DisplayMode::kUndefined;
  switch (window_mode) {
    case apps::mojom::WindowMode::kUnknown:
      display_mode = blink::mojom::DisplayMode::kUndefined;
      break;
    case apps::mojom::WindowMode::kBrowser:
      display_mode = blink::mojom::DisplayMode::kBrowser;
      break;
    case apps::mojom::WindowMode::kWindow:
      display_mode = blink::mojom::DisplayMode::kStandalone;
      break;
    case apps::mojom::WindowMode::kTabbedWindow:
      display_mode = blink::mojom::DisplayMode::kTabbed;
      break;
  }
  provider_->sync_bridge().SetAppUserDisplayMode(app_id, display_mode,
                                                 /*is_user_action=*/true);
}

apps::mojom::WindowMode WebAppPublisherHelper::ConvertDisplayModeToWindowMode(
    blink::mojom::DisplayMode display_mode) {
  switch (display_mode) {
    case blink::mojom::DisplayMode::kUndefined:
      return apps::mojom::WindowMode::kUnknown;
    case blink::mojom::DisplayMode::kBrowser:
      return apps::mojom::WindowMode::kBrowser;
    case blink::mojom::DisplayMode::kTabbed:
      return apps::mojom::WindowMode::kTabbedWindow;
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kFullscreen:
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
      return apps::mojom::WindowMode::kWindow;
  }
}

void WebAppPublisherHelper::PublishWindowModeUpdate(
    const std::string& app_id,
    blink::mojom::DisplayMode display_mode) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type();
  app->app_id = app_id;
  app->window_mode = ConvertDisplayModeToWindowMode(display_mode);
  delegate_->PublishWebApp(std::move(app));
}

std::string WebAppPublisherHelper::GenerateShortcutId() {
  return base::NumberToString(shortcut_id_generator_.GenerateNextId().value());
}

void WebAppPublisherHelper::StoreShortcutId(
    const std::string& shortcut_id,
    const WebApplicationShortcutsMenuItemInfo& menu_item_info) {
  shortcut_id_map_.emplace(shortcut_id, std::move(menu_item_info));
}

content::WebContents* WebAppPublisherHelper::ExecuteContextMenuCommand(
    const std::string& app_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return nullptr;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params(
      app_id, ConvertDisplayModeToAppLaunchContainer(display_mode),
      WindowOpenDisposition::CURRENT_TAB,
      apps::mojom::AppLaunchSource::kSourceAppLauncher, display_id);

  auto menu_item = shortcut_id_map_.find(shortcut_id);
  if (menu_item != shortcut_id_map_.end()) {
    params.override_url = menu_item->second.url;
  }

  return LaunchAppWithParams(std::move(params));
}

WebAppRegistrar& WebAppPublisherHelper::registrar() const {
  return provider_->registrar();
}

void WebAppPublisherHelper::OnWebAppInstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    delegate_->PublishWebApp(ConvertWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppInstalledWithOsHooks(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    delegate_->PublishWebApp(ConvertWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppManifestUpdated(
    const AppId& app_id,
    base::StringPiece old_name) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    delegate_->PublishWebApp(ConvertWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppWillBeUninstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  paused_apps_.MaybeRemoveApp(app_id);

#if defined(OS_CHROMEOS)
  app_notifications_.RemoveNotificationsForApp(app_id);

  auto result = media_requests_.RemoveRequests(app_id);
  delegate_->ModifyWebAppCapabilityAccess(app_id, result.camera,
                                          result.microphone);
#endif

  delegate_->PublishWebApp(ConvertUninstalledWebApp(web_app));
}

void WebAppPublisherHelper::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void WebAppPublisherHelper::OnWebAppLocallyInstalledStateChanged(
    const AppId& app_id,
    bool is_locally_installed) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  delegate_->PublishWebApp(ConvertWebApp(web_app));
}

void WebAppPublisherHelper::OnWebAppLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  delegate_->PublishWebApp(ConvertLaunchedWebApp(web_app));
}

void WebAppPublisherHelper::OnWebAppUserDisplayModeChanged(
    const AppId& app_id,
    DisplayMode user_display_mode) {
  PublishWindowModeUpdate(app_id, user_display_mode);
}

#if defined(OS_CHROMEOS)
// If is_disabled is set, the app backed by |app_id| is published with readiness
// kDisabledByPolicy, otherwise it's published with readiness kReady.
void WebAppPublisherHelper::OnWebAppDisabledStateChanged(const AppId& app_id,
                                                         bool is_disabled) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  DCHECK_EQ(is_disabled, web_app->chromeos_data()->is_disabled);
  apps::mojom::AppPtr app = ConvertWebApp(web_app);
  app->icon_key = MakeIconKey(web_app);

  // If the disable mode is hidden, update the visibility of the new disabled
  // app.
  if (is_disabled && provider_->policy_manager().IsDisabledAppsModeHidden()) {
    UpdateAppDisabledMode(app);
  }

  delegate_->PublishWebApp(std::move(app));
}

void WebAppPublisherHelper::OnWebAppsDisabledModeChanged() {
  std::vector<apps::mojom::AppPtr> apps;
  std::vector<AppId> app_ids = registrar().GetAppIds();
  for (const auto& id : app_ids) {
    // We only update visibility of disabled apps in this method. When enabling
    // previously disabled app, OnWebAppDisabledStateChanged() method will be
    // called and this method will update visibility and readiness of the newly
    // enabled app.
    if (provider_->policy_manager().IsWebAppInDisabledList(id)) {
      const WebApp* web_app = GetWebApp(id);
      if (!web_app || !Accepts(id)) {
        continue;
      }
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = app_type();
      app->app_id = web_app->app_id();
      UpdateAppDisabledMode(app);
      apps.push_back(std::move(app));
    }
  }
  delegate_->PublishWebApps(std::move(apps));
}
#endif

#if defined(OS_CHROMEOS)
void WebAppPublisherHelper::OnNotificationDisplayed(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  if (notification.notifier_id().type !=
      message_center::NotifierType::WEB_PAGE) {
    return;
  }
  MaybeAddWebPageNotifications(notification, metadata);
}

void WebAppPublisherHelper::OnNotificationClosed(
    const std::string& notification_id) {
  auto app_ids = app_notifications_.GetAppIdsForNotification(notification_id);
  if (app_ids.empty()) {
    return;
  }

  app_notifications_.RemoveNotification(notification_id);

  for (const auto& app_id : app_ids) {
    apps::mojom::AppPtr app =
        app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id);
    app->has_badge = ShouldShowBadge(app_id, app->has_badge);
    delegate_->PublishWebApp(std::move(app));
  }
}

void WebAppPublisherHelper::OnNotificationDisplayServiceDestroyed(
    NotificationDisplayService* service) {
  DCHECK(notification_display_service_.IsObservingSource(service));
  notification_display_service_.Reset();
}

void WebAppPublisherHelper::OnRequestUpdate(
    int render_process_id,
    int render_frame_id,
    blink::mojom::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));

  if (!web_contents) {
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
  delegate_->ModifyWebAppCapabilityAccess(app_id.value(), result.camera,
                                          result.microphone);
}

void WebAppPublisherHelper::OnWebContentsDestroyed(
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
  delegate_->ModifyWebAppCapabilityAccess(app_id.value(), result.camera,
                                          result.microphone);
}
#endif

void WebAppPublisherHelper::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // If content_type is not one of the supported permissions, do nothing.
  if (!IsSupportedWebAppPermissionType(content_type)) {
    return;
  }

  for (const WebApp& web_app : registrar().GetApps()) {
    if (primary_pattern.Matches(web_app.start_url()) &&
        Accepts(web_app.app_id())) {
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = app_type_;
      app->app_id = web_app.app_id();
      PopulateWebAppPermissions(&web_app, &app->permissions);

      delegate_->PublishWebApp(std::move(app));
    }
  }
}

void WebAppPublisherHelper::Init(bool observe_media_requests) {
  // Allow for web app migration tests.
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  registrar_observation_.Observe(&registrar());
  content_settings_observation_.Observe(
      HostContentSettingsMapFactory::GetForProfile(profile_));

#if defined(OS_CHROMEOS)
  notification_display_service_.Observe(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  badge_manager_ = badging::BadgeManagerFactory::GetForProfile(profile());
  // badge_manager_ is nullptr in guest and incognito profiles.
  if (badge_manager_) {
    badge_manager_->SetDelegate(
        std::make_unique<WebAppPublisherHelper::BadgeManagerDelegate>(
            weak_ptr_factory_.GetWeakPtr()));
  }
#endif

  web_app_launch_manager_ = std::make_unique<WebAppLaunchManager>(profile_);

#if defined(OS_CHROMEOS)
  if (observe_media_requests) {
    media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance());
  }
#endif
}

IconEffects WebAppPublisherHelper::GetIconEffects(const WebApp* web_app) {
  IconEffects icon_effects = IconEffects::kRoundCorners;
  if (!web_app->is_locally_installed()) {
    icon_effects |= IconEffects::kBlocked;
  }

#if defined(OS_CHROMEOS)
  icon_effects |= web_app->is_generated_icon() ? IconEffects::kCrOsStandardMask
                                               : IconEffects::kCrOsStandardIcon;
#endif

  if (IsPaused(web_app->app_id())) {
    icon_effects |= IconEffects::kPaused;
  }

  bool is_disabled = false;
  if (web_app->chromeos_data().has_value()) {
    is_disabled = web_app->chromeos_data()->is_disabled;
  }
  if (is_disabled) {
    icon_effects |= IconEffects::kBlocked;
  }

  return icon_effects;
}

const WebApp* WebAppPublisherHelper::GetWebApp(const AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

content::WebContents* WebAppPublisherHelper::MaybeNavigateExistingWindow(
    const std::string& app_id,
    absl::optional<GURL> url) {
  content::WebContents* web_contents = nullptr;
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return web_contents;
  }
  if (web_app->capture_links() ==
      blink::mojom::CaptureLinks::kExistingClientNavigate) {
    web_contents = provider_->ui_manager().NavigateExistingWindow(
        app_id, url ? url.value() : registrar().GetAppLaunchUrl(app_id));
  }
  return web_contents;
}

content::WebContents* WebAppPublisherHelper::LaunchAppWithIntentImpl(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  if (!profile_) {
    return nullptr;
  }

  content::WebContents* web_contents =
      MaybeNavigateExistingWindow(app_id, intent->url);
  if (web_contents) {
    return web_contents;
  }

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, event_flags, apps::GetAppLaunchSource(launch_source), display_id,
      ConvertDisplayModeToAppLaunchContainer(
          registrar().GetAppEffectiveDisplayMode(app_id)),
      std::move(intent));
  return LaunchAppWithParams(std::move(params));
}

#if defined(OS_CHROMEOS)
void WebAppPublisherHelper::UpdateAppDisabledMode(apps::mojom::AppPtr& app) {
  if (provider_->policy_manager().IsDisabledAppsModeHidden()) {
    app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
    app->show_in_search = apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = apps::mojom::OptionalBool::kFalse;
    return;
  }
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto system_app_type =
      provider_->system_web_app_manager().GetSystemAppTypeForAppId(app->app_id);
  if (system_app_type.has_value()) {
    auto* system_app =
        provider_->system_web_app_manager().GetSystemApp(*system_app_type);
    DCHECK(system_app);
    app->show_in_launcher = system_app->ShouldShowInLauncher()
                                ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;
    app->show_in_search = system_app->ShouldShowInSearch()
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
  }
#endif
}

bool WebAppPublisherHelper::MaybeAddNotification(
    const std::string& app_id,
    const std::string& notification_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return false;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  apps::mojom::AppPtr app =
      app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id);
  app->has_badge = ShouldShowBadge(app_id, app->has_badge);
  delegate_->PublishWebApp(std::move(app));
  return true;
}

void WebAppPublisherHelper::MaybeAddWebPageNotifications(
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

    const GURL& url = non_persistent_metadata &&
                              !non_persistent_metadata->document_url.is_empty()
                          ? non_persistent_metadata->document_url
                          : notification.origin_url();

    auto app_ids = registrar().FindAppsInScope(url);
    int count = 0;
    for (const auto& app_id : app_ids) {
      if (MaybeAddNotification(app_id, notification.id())) {
        ++count;
      }
    }
    apps::RecordAppsPerNotification(count);
  }
}

apps::mojom::OptionalBool WebAppPublisherHelper::ShouldShowBadge(
    const std::string& app_id,
    apps::mojom::OptionalBool has_notification) {
  // We show a badge if either the Web Badging API recently has a badge set, or
  // the Badging API has not been recently used by the app and a notification is
  // showing.
  if (!badge_manager_ || !badge_manager_->HasRecentApiUsage(app_id))
    return has_notification;

  return badge_manager_->GetBadgeValue(app_id).has_value()
             ? apps::mojom::OptionalBool::kTrue
             : apps::mojom::OptionalBool::kFalse;
}
#endif

}  // namespace web_app
