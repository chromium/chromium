// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <ostream>
#include <set>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/one_shot_event.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chrome/browser/web_applications/commands/compute_app_size_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/cookies/cookie_partition_key.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"  // nogncheck
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"  // nogncheck
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/api/file_handlers/mime_util.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

using apps::IconEffects;

namespace content {
class BrowserContext;
}

namespace ui {
enum ResourceScaleFactor : int;
}

namespace web_app {

namespace {

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

// Mime Type for plain text.
const char kTextPlain[] = "text/plain";

bool GetContentSettingsType(apps::PermissionType permission_type,
                            ContentSettingsType& content_setting_type) {
  switch (permission_type) {
    case apps::PermissionType::kCamera:
      content_setting_type = ContentSettingsType::MEDIASTREAM_CAMERA;
      return true;
    case apps::PermissionType::kLocation:
      content_setting_type = ContentSettingsType::GEOLOCATION;
      return true;
    case apps::PermissionType::kMicrophone:
      content_setting_type = ContentSettingsType::MEDIASTREAM_MIC;
      return true;
    case apps::PermissionType::kNotifications:
      content_setting_type = ContentSettingsType::NOTIFICATIONS;
      return true;
    case apps::PermissionType::kUnknown:
    case apps::PermissionType::kContacts:
    case apps::PermissionType::kStorage:
    case apps::PermissionType::kPrinting:
    case apps::PermissionType::kFileHandling:
      return false;
  }
}

apps::PermissionType GetPermissionType(
    ContentSettingsType content_setting_type) {
  switch (content_setting_type) {
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return apps::PermissionType::kCamera;
    case ContentSettingsType::GEOLOCATION:
      return apps::PermissionType::kLocation;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return apps::PermissionType::kMicrophone;
    case ContentSettingsType::NOTIFICATIONS:
      return apps::PermissionType::kNotifications;
    default:
      return apps::PermissionType::kUnknown;
  }
}

apps::InstallReason GetHighestPriorityInstallReason(const WebApp* web_app) {
  // TODO(crbug.com/40755721): Migrate apps with chromeos_data.oem_installed set
  // to the new WebAppManagement::Type::kOem install type.
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    if (chromeos_data.oem_installed) {
      DCHECK(!web_app->IsSystemApp());
      return apps::InstallReason::kOem;
    }
  }

  // We do not make a distinction in `apps::InstallReason` between IWA sources
  // and non-IWA sources. For example, we map both `WebAppManagement::kPolicy`
  // and `WebAppManagement::kIwaPolicy` to `apps::InstallReason::kPolicy`. This
  // is only possible because there is only a one-way conversion from
  // `WebAppManagement::Type` to `apps::InstallReason`. Should we ever make them
  // convertible in the other direction, we'd need to add IWA-specific sources
  // to `apps::InstallReason` first.
  switch (web_app->GetHighestPrioritySource()) {
    case WebAppManagement::kSystem:
    case WebAppManagement::kIwaShimlessRma:
      return apps::InstallReason::kSystem;
    case WebAppManagement::kKiosk:
      return apps::InstallReason::kKiosk;
    case WebAppManagement::kPolicy:
    case WebAppManagement::kIwaPolicy:
      return apps::InstallReason::kPolicy;
    case WebAppManagement::kOem:
      return apps::InstallReason::kOem;
    case WebAppManagement::kSubApp:
      return apps::InstallReason::kSubApp;
    case WebAppManagement::kWebAppStore:
    case WebAppManagement::kOneDriveIntegration:
    case WebAppManagement::kIwaUserInstalled:
    case WebAppManagement::kUserInstalled:
      return apps::InstallReason::kUser;
    case WebAppManagement::kSync:
      return apps::InstallReason::kSync;
    case WebAppManagement::kDefault:
    case WebAppManagement::kApsDefault:
      return apps::InstallReason::kDefault;
  }
}

apps::InstallSource GetInstallSource(
    std::optional<webapps::WebappInstallSource> source) {
  if (!source) {
    return apps::InstallSource::kUnknown;
  }

  switch (*source) {
    case webapps::WebappInstallSource::MENU_BROWSER_TAB:
    case webapps::WebappInstallSource::MENU_CUSTOM_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case webapps::WebappInstallSource::API_BROWSER_TAB:
    case webapps::WebappInstallSource::API_CUSTOM_TAB:
    case webapps::WebappInstallSource::DEVTOOLS:
    case webapps::WebappInstallSource::MANAGEMENT_API:
    case webapps::WebappInstallSource::IWA_DEV_UI:
    case webapps::WebappInstallSource::IWA_DEV_COMMAND_LINE:
    case webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER:
    case webapps::WebappInstallSource::IWA_EXTERNAL_POLICY:
    case webapps::WebappInstallSource::IWA_SHIMLESS_RMA:
    case webapps::WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case webapps::WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case webapps::WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
    case webapps::WebappInstallSource::EXTERNAL_POLICY:
    case webapps::WebappInstallSource::ML_PROMOTION:
    case webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case webapps::WebappInstallSource::MENU_CREATE_SHORTCUT:
    case webapps::WebappInstallSource::SUB_APP:
    case webapps::WebappInstallSource::CHROME_SERVICE:
    case webapps::WebappInstallSource::KIOSK:
    case webapps::WebappInstallSource::MICROSOFT_365_SETUP:
    case webapps::WebappInstallSource::PROFILE_MENU:
    case webapps::WebappInstallSource::ALMANAC_INSTALL_APP_URI:
    case webapps::WebappInstallSource::OOBE_APP_RECOMMENDATIONS:
      return apps::InstallSource::kBrowser;
    case webapps::WebappInstallSource::ARC:
      return apps::InstallSource::kPlayStore;
    case webapps::WebappInstallSource::INTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_LOCK_SCREEN:
    case webapps::WebappInstallSource::SYSTEM_DEFAULT:
    case webapps::WebappInstallSource::PRELOADED_OEM:
    case webapps::WebappInstallSource::PRELOADED_DEFAULT:
      return apps::InstallSource::kSystem;
    case webapps::WebappInstallSource::SYNC:
    case webapps::WebappInstallSource::WEBAPK_RESTORE:
      return apps::InstallSource::kSync;
    case webapps::WebappInstallSource::COUNT:
      NOTREACHED_IN_MIGRATION();
      return apps::InstallSource::kUnknown;
  }
}

apps::Readiness ConvertWebappUninstallSourceToReadiness(
    webapps::WebappUninstallSource source) {
  switch (source) {
    case webapps::WebappUninstallSource::kUnknown:
    case webapps::WebappUninstallSource::kAppMenu:
    case webapps::WebappUninstallSource::kAppsPage:
    case webapps::WebappUninstallSource::kOsSettings:
    case webapps::WebappUninstallSource::kSync:
    case webapps::WebappUninstallSource::kAppManagement:
    case webapps::WebappUninstallSource::kAppList:
    case webapps::WebappUninstallSource::kShelf:
    case webapps::WebappUninstallSource::kPlaceholderReplacement:
    case webapps::WebappUninstallSource::kArc:
    case webapps::WebappUninstallSource::kSubApp:
    case webapps::WebappUninstallSource::kStartupCleanup:
    case webapps::WebappUninstallSource::kParentUninstall:
    case webapps::WebappUninstallSource::kTestCleanup:
    case webapps::WebappUninstallSource::kDevtools:
      return apps::Readiness::kUninstalledByUser;
    case webapps::WebappUninstallSource::kMigration:
    case webapps::WebappUninstallSource::kInternalPreinstalled:
    case webapps::WebappUninstallSource::kExternalPreinstalled:
    case webapps::WebappUninstallSource::kExternalPolicy:
    case webapps::WebappUninstallSource::kSystemPreinstalled:
    case webapps::WebappUninstallSource::kExternalLockScreen:
    case webapps::WebappUninstallSource::kInstallUrlDeduping:
    case webapps::WebappUninstallSource::kHealthcareUserInstallCleanup:
    case webapps::WebappUninstallSource::kIwaEnterprisePolicy:
      return apps::Readiness::kUninstalledByNonUser;
  }
}

bool IsNoteTakingWebApp(const WebApp& web_app) {
  return web_app.note_taking_new_note_url().is_valid();
}

bool IsLockScreenCapable(const WebApp& web_app) {
  if (!base::FeatureList::IsEnabled(features::kWebLockScreenApi)) {
    return false;
  }
  return web_app.lock_screen_start_url().is_valid();
}

apps::IntentFilterPtr CreateMimeTypeShareFilter(
    const std::vector<std::string>& mime_types) {
  DCHECK(!mime_types.empty());
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  std::vector<apps::ConditionValuePtr> action_condition_values;
  action_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::kIntentActionSend, apps::PatternMatchType::kLiteral));
  auto action_condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(action_condition_values));
  intent_filter->conditions.push_back(std::move(action_condition));

  std::vector<apps::ConditionValuePtr> condition_values;
  for (auto& mime_type : mime_types) {
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        mime_type, apps::PatternMatchType::kMimeType));
  }
  auto mime_condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kMimeType, std::move(condition_values));
  intent_filter->conditions.push_back(std::move(mime_condition));

  return intent_filter;
}

apps::IntentFilterPtr CreateIntentFilterFromOrigin(
    const url::Origin& origin,
    bool add_subdomain_wildcard) {
  CHECK(!origin.opaque());

  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                         apps_util::kIntentActionView,
                                         apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme,
                                         origin.scheme(),
                                         apps::PatternMatchType::kLiteral);

  std::string authority = apps_util::AuthorityView::Encode(origin);
  if (add_subdomain_wildcard) {
    DCHECK(!base::StartsWith(authority, "."));
    authority = '.' + authority;
  }
  intent_filter->AddSingleValueCondition(
      apps::ConditionType::kAuthority, authority,
      add_subdomain_wildcard ? apps::PatternMatchType::kSuffix
                             : apps::PatternMatchType::kLiteral);

  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, "",
                                         apps::PatternMatchType::kPrefix);

  return intent_filter;
}

apps::IntentFilters CreateIntentFiltersFromScopeExtensionInfo(
    const web_app::ScopeExtensionInfo& scope_extension_info) {
  apps::IntentFilters filters;
  filters.push_back(
      CreateIntentFilterFromOrigin(scope_extension_info.origin,
                                   /*add_subdomain_wildcard=*/false));
  if (scope_extension_info.has_origin_wildcard) {
    // In addition to matching the exact same origin, the wildcard should match
    // subdomains.
    filters.push_back(CreateIntentFilterFromOrigin(
        scope_extension_info.origin, /*add_subdomain_wildcard=*/true));
  }
  return filters;
}

apps::IntentFilters CreateShareIntentFiltersFromShareTarget(
    const apps::ShareTarget& share_target) {
  apps::IntentFilters filters;

  if (!share_target.params.text.empty()) {
    // The share target accepts navigator.share() calls with text.
    filters.push_back(CreateMimeTypeShareFilter({kTextPlain}));
  }

  std::vector<std::string> content_types;
  for (const auto& files_entry : share_target.params.files) {
    for (const auto& file_type : files_entry.accept) {
      // Skip any file_type that is not a MIME type.
      if (file_type.empty() || file_type[0] == '.' ||
          base::ranges::count(file_type, '/') != 1) {
        continue;
      }

      content_types.push_back(file_type);
    }
  }

  if (!content_types.empty()) {
    const std::vector<std::string> intent_actions(
        {apps_util::kIntentActionSend, apps_util::kIntentActionSendMultiple});
    filters.push_back(
        apps_util::CreateFileFilter(intent_actions, content_types, {}));
  }

  return filters;
}

apps::IntentFilters CreateIntentFiltersFromFileHandlers(
    const apps::FileHandlers& file_handlers) {
  apps::IntentFilters filters;
  for (const apps::FileHandler& handler : file_handlers) {
    std::vector<std::string> mime_types;
    std::vector<std::string> file_extensions;
    std::string action_url = handler.action.spec();
    // TODO(petermarshall): Use GetFileExtensionsFromFileHandlers /
    // GetMimeTypesFromFileHandlers?
    for (const apps::FileHandler::AcceptEntry& accept_entry : handler.accept) {
      mime_types.push_back(accept_entry.mime_type);
      for (const std::string& extension : accept_entry.file_extensions) {
        file_extensions.push_back(extension);
      }
    }
    filters.push_back(
        apps_util::CreateFileFilter({apps_util::kIntentActionView}, mime_types,
                                    file_extensions, action_url));
  }

  return filters;
}

}  // namespace

void UninstallImpl(WebAppProvider* provider,
                   const std::string& app_id,
                   apps::UninstallSource uninstall_source,
                   gfx::NativeWindow parent_window) {
  if (!provider) {
    return;
  }

  if (provider->registrar_unsafe().CanUserUninstallWebApp(app_id)) {
    webapps::WebappUninstallSource webapp_uninstall_source =
        ConvertUninstallSourceToWebAppUninstallSource(uninstall_source);
    provider->ui_manager().PresentUserUninstallDialog(
        app_id, webapp_uninstall_source, parent_window, base::DoNothing());
  }
}

WebAppPublisherHelper::Delegate::Delegate() = default;

WebAppPublisherHelper::Delegate::~Delegate() = default;

#if BUILDFLAG(IS_CHROMEOS)
WebAppPublisherHelper::BadgeManagerDelegate::BadgeManagerDelegate(
    const base::WeakPtr<WebAppPublisherHelper>& publisher_helper)
    : badging::BadgeManagerDelegate(publisher_helper->profile(),
                                    publisher_helper->badge_manager_),
      publisher_helper_(publisher_helper) {}

WebAppPublisherHelper::BadgeManagerDelegate::~BadgeManagerDelegate() = default;

void WebAppPublisherHelper::BadgeManagerDelegate::OnAppBadgeUpdated(
    const webapps::AppId& app_id) {
  if (!publisher_helper_) {
    return;
  }
  apps::AppPtr app =
      publisher_helper_->app_notifications_.CreateAppWithHasBadgeStatus(
          publisher_helper_->app_type(), app_id);
  DCHECK(app->has_badge.has_value());
  app->has_badge =
      publisher_helper_->ShouldShowBadge(app_id, app->has_badge.value());
  publisher_helper_->delegate_->PublishWebApp(std::move(app));
}
#endif

WebAppPublisherHelper::WebAppPublisherHelper(Profile* profile,
                                             WebAppProvider* provider,
                                             Delegate* delegate)
    : profile_(profile),
      provider_(provider),
      app_type_(GetWebAppType()),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(delegate_);
  Init();
}

WebAppPublisherHelper::~WebAppPublisherHelper() = default;

// static
apps::AppType WebAppPublisherHelper::GetWebAppType() {
// After moving the ordinary Web Apps to Lacros chrome, the remaining web
// apps in ash Chrome will be only System Web Apps. Change the app type
// to kSystemWeb for this case and the kWeb app type will be published from
// the publisher for Lacros web apps.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled() && IsWebAppsCrosapiEnabled()) {
    return apps::AppType::kSystemWeb;
  }
#endif

  return apps::AppType::kWeb;
}

// static
bool WebAppPublisherHelper::IsSupportedWebAppPermissionType(
    ContentSettingsType permission_type) {
  return base::Contains(kSupportedPermissionTypes, permission_type);
}

void WebAppPublisherHelper::Shutdown() {
  registrar_observation_.Reset();
  content_settings_observation_.Reset();
  is_shutting_down_ = true;
}

void WebAppPublisherHelper::SetWebAppShowInFields(const WebApp* web_app,
                                                  apps::App& app) {
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    bool should_show_app = true;
    // TODO(b/201422755): Remove Web app specific hiding for demo mode once icon
    // load fixed.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (ash::DemoSession::Get()) {
      should_show_app = ash::DemoSession::Get()->ShouldShowWebApp(
          web_app->start_url().spec());
    }
#endif
    app.show_in_launcher = chromeos_data.show_in_launcher && should_show_app;
    app.show_in_shelf = app.show_in_search =
        chromeos_data.show_in_search_and_shelf && should_show_app;
    app.show_in_management = chromeos_data.show_in_management;
    app.handles_intents =
        chromeos_data.handles_file_open_intents ? true : app.show_in_launcher;
    return;
  }

  // Show the app everywhere by default.
  app.show_in_launcher = true;
  app.show_in_shelf = true;
  app.show_in_search = true;
  app.show_in_management = true;
  app.handles_intents = true;
}

apps::Permissions WebAppPublisherHelper::CreatePermissions(
    const WebApp* web_app) {
  apps::Permissions permissions;

  const GURL& url = web_app->start_url();
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting =
        host_content_settings_map->GetContentSetting(url, url, type);

    // Map ContentSettingsType to an apps::TriState value
    apps::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::TriState::kBlock;
        break;
      default:
        setting_val = apps::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, &setting_info);

    permissions.push_back(std::make_unique<apps::Permission>(
        GetPermissionType(type), setting_val,
        /*is_managed=*/setting_info.source ==
            content_settings::SettingSource::kPolicy));
  }

  // File handling permission.
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kFileHandling,
      !registrar().IsAppFileHandlerPermissionBlocked(web_app->app_id()),
      /*is_managed=*/false));

  return permissions;
}

// static
apps::IntentFilters WebAppPublisherHelper::CreateIntentFiltersForWebApp(
    const WebAppProvider& provider,
    const web_app::WebApp& app) {
  apps::IntentFilters filters;

  GURL app_scope = provider.registrar_unsafe().GetAppScope(app.app_id());
  if (!app_scope.is_empty()) {
    filters.push_back(apps_util::MakeIntentFilterForUrlScope(app_scope));
  }

  for (const ScopeExtensionInfo& scope_extension_info :
       app.validated_scope_extensions()) {
    base::Extend(filters, CreateIntentFiltersFromScopeExtensionInfo(
                              scope_extension_info));
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    for (const ScopeExtensionInfo& scope_extension_info :
         ChromeOsWebAppExperiments::GetScopeExtensions(app.app_id())) {
      base::Extend(filters, CreateIntentFiltersFromScopeExtensionInfo(
                                scope_extension_info));
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (app.share_target()) {
    base::Extend(filters,
                 CreateShareIntentFiltersFromShareTarget(*app.share_target()));
  }

  const apps::FileHandlers* enabled_file_handlers =
      provider.os_integration_manager().GetEnabledFileHandlers(app.app_id());
  if (enabled_file_handlers) {
    base::Extend(filters,
                 CreateIntentFiltersFromFileHandlers(*enabled_file_handlers));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app.app_id() == ash::kChromeUIUntrustedProjectorSwaAppId) {
    filters.push_back(apps_util::MakeIntentFilterForUrlScope(
        GURL(ash::kChromeUIUntrustedProjectorPwaUrl)));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return filters;
}

apps::AppPtr WebAppPublisherHelper::CreateWebApp(const WebApp* web_app) {
  DCHECK(!IsShuttingDown());

  apps::Readiness readiness;

  switch (web_app->install_state()) {
    case proto::InstallState::INSTALLED_WITH_OS_INTEGRATION:
    case proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION:
      readiness =
          (web_app->is_uninstalling() ? apps::Readiness::kUninstalledByUser
                                      : apps::Readiness::kReady);
      break;
    case proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE:
      readiness = apps::Readiness::kDisabledByUser;
  }

#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(web_app->chromeos_data().has_value());
  if (web_app->chromeos_data()->is_disabled) {
    readiness = apps::Readiness::kDisabledByPolicy;
  }
#endif

  auto app = apps::AppPublisher::MakeApp(
      app_type(), web_app->app_id(), readiness,
      provider_->registrar_unsafe().GetAppShortName(web_app->app_id()),
      GetHighestPriorityInstallReason(web_app),
      GetInstallSource(provider_->registrar_unsafe().GetLatestAppInstallSource(
          web_app->app_id())));

  app->description =
      provider_->registrar_unsafe().GetAppDescription(web_app->app_id());
  if (web_app->isolation_data().has_value()) {
    // Show the version of Isolated Web App in ChromeOS Settings
    app->version = web_app->isolation_data()->version().GetString();
  }

  app->additional_search_terms = web_app->additional_search_terms();

  // Web App's publisher_id the start url.
  app->publisher_id = web_app->start_url().spec();
  app->installer_package_id = GetPackageId(*web_app);

  app->icon_key = apps::IconKey(GetIconEffects(web_app));

  app->last_launch_time = web_app->last_launch_time();
  app->install_time = web_app->first_install_time();

  // For system web apps and shimless RMA IWAs (only), the install source is
  // `kSystem`.
  DCHECK_EQ(web_app->IsSystemApp() || web_app->IsIwaShimlessRmaApp(),
            app->install_reason == apps::InstallReason::kSystem)
      << base::ToString(app->install_reason);

  app->policy_ids = GetPolicyIds(*web_app);

  app->permissions = CreatePermissions(web_app);

  // Isolated web apps can only be opened in window.
  app->allow_window_mode_selection = !web_app->isolation_data().has_value();

  SetWebAppShowInFields(web_app, *app);

#if BUILDFLAG(IS_CHROMEOS)
  if (readiness != apps::Readiness::kReady) {
    UpdateAppDisabledMode(*app);
  }

  app->has_badge = ShouldShowBadge(
      web_app->app_id(), app_notifications_.HasNotification(web_app->app_id()));
#else
  app->has_badge = false;
#endif

  app->allow_uninstall = web_app->CanUserUninstallWebApp();

#if BUILDFLAG(IS_CHROMEOS)
  app->paused = IsPaused(web_app->app_id());
#else
  app->paused = false;
#endif

  // Add the intent filters for PWAs.
  base::Extend(app->intent_filters,
               CreateIntentFiltersForWebApp(*provider_, *web_app));

  // These filters are used by the settings page to display would-be-handled
  // extensions even when the feature is not enabled for the app, whereas
  // `GetEnabledFileHandlers` above only returns the ones that currently are
  // enabled.
  const apps::FileHandlers* all_file_handlers =
      registrar().GetAppFileHandlers(web_app->app_id());
  if (all_file_handlers && !all_file_handlers->empty()) {
    std::set<std::string> extensions_set =
        apps::GetFileExtensionsFromFileHandlers(*all_file_handlers);
    app->intent_filters.push_back(apps_util::CreateFileFilter(
        {apps_util::kIntentActionPotentialFileHandler},
        /*mime_types=*/{},
        /*file_extensions=*/
        {extensions_set.begin(), extensions_set.end()}));
  }

  if (IsNoteTakingWebApp(*web_app)) {
    app->intent_filters.push_back(apps_util::CreateNoteTakingFilter());
  }

  if (IsLockScreenCapable(*web_app)) {
    app->intent_filters.push_back(apps_util::CreateLockScreenFilter());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (web_app->app_id() == guest_os::kTerminalSystemAppId) {
    app->intent_filters.push_back(apps_util::CreateFileFilter(
        {apps_util::kIntentActionView},
        /*mime_types=*/
        {extensions::app_file_handler_util::kMimeTypeInodeDirectory},
        /*file_extensions=*/{}));
  }
#endif

  app->window_mode = ConvertDisplayModeToWindowMode(
      registrar().GetAppEffectiveDisplayMode(web_app->app_id()));

  const auto login_mode = registrar().GetAppRunOnOsLoginMode(web_app->app_id());
  app->run_on_os_login = apps::RunOnOsLogin(
      ConvertOsLoginMode(login_mode.value), !login_mode.user_controllable);

  app->allow_close = !registrar().IsPreventCloseEnabled(web_app->app_id());

  for (const auto& shortcut : web_app->shortcuts_menu_item_infos()) {
    const std::string name = base::UTF16ToUTF8(shortcut.name);
    std::string shortcut_id = GenerateShortcutId();
    StoreShortcutId(shortcut_id, shortcut);
  }

  return app;
}

apps::AppPtr WebAppPublisherHelper::ConvertUninstalledWebApp(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  auto app = std::make_unique<apps::App>(app_type(), app_id);
  app->readiness = ConvertWebappUninstallSourceToReadiness(uninstall_source);

  return app;
}

apps::AppPtr WebAppPublisherHelper::ConvertLaunchedWebApp(
    const WebApp* web_app) {
  auto app = std::make_unique<apps::App>(app_type(), web_app->app_id());
  app->last_launch_time = web_app->last_launch_time();
  return app;
}

void WebAppPublisherHelper::UninstallWebApp(
    const WebApp* web_app,
    apps::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  if (IsShuttingDown()) {
    return;
  }

  auto origin = url::Origin::Create(web_app->start_url());

  DCHECK(provider_);
  DCHECK(
      provider_->registrar_unsafe().CanUserUninstallWebApp(web_app->app_id()));
  webapps::WebappUninstallSource webapp_uninstall_source =
      ConvertUninstallSourceToWebAppUninstallSource(uninstall_source);
  provider_->scheduler().RemoveUserUninstallableManagements(
      web_app->app_id(), webapp_uninstall_source, base::DoNothing());
  web_app = nullptr;

  if (!clear_site_data) {
    return;
  }

  // Off the record profiles cannot be 'kept alive'.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive =
      profile_->IsOffTheRecord()
          ? nullptr
          : std::make_unique<ScopedProfileKeepAlive>(
                profile_, ProfileKeepAliveOrigin::kWebAppUninstall);
  // Ensure profile is kept alive until ClearSiteData is done.
  auto callback = base::BindOnce(
      [](std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive) {},
      std::move(profile_keep_alive));
  content::ClearSiteData(
      profile()->GetWeakPtr(),
      /*storage_partition_config=*/std::nullopt, origin,
      content::ClearSiteDataTypeSet::All(),
      /*storage_buckets_to_remove=*/{}, /*avoid_closing_connections=*/false,
      /*cookie_partition_key=*/std::nullopt,
      /*storage_key=*/std::nullopt,
      /*partitioned_state_allowed_only=*/false, std::move(callback));
}

void WebAppPublisherHelper::SetIconEffect(const std::string& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto app = std::make_unique<apps::App>(app_type(), app_id);
  app->icon_key = apps::IconKey(GetIconEffects(web_app));
  delegate_->PublishWebApp(std::move(app));
}

#if BUILDFLAG(IS_CHROMEOS)
void WebAppPublisherHelper::PauseApp(const std::string& app_id) {
  if (IsShuttingDown()) {
    return;
  }

  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }

  if (!IsWebAppsCrosapiEnabled()) {
    provider_->ui_manager().CloseAppWindows(app_id);
  } else {
    CHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
        profile_));

    apps::BrowserAppInstanceTracker* instance_tracker =
        apps::AppServiceProxyFactory::GetForProfile(profile_)
            ->BrowserAppInstanceTracker();
    CHECK(instance_tracker);

    instance_tracker->StopInstancesOfApp(app_id);
  }

  delegate_->PublishWebApp(paused_apps_.CreateAppWithPauseStatus(
      app_type(), app_id, /*paused=*/true));
}

void WebAppPublisherHelper::UnpauseApp(const std::string& app_id) {
  if (IsShuttingDown()) {
    return;
  }

  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  delegate_->PublishWebApp(paused_apps_.CreateAppWithPauseStatus(
      app_type(), app_id, /*paused=*/false));
}

bool WebAppPublisherHelper::IsPaused(const std::string& app_id) {
  return paused_apps_.IsPaused(app_id);
}

void WebAppPublisherHelper::StopApp(const std::string& app_id) {
  if (IsShuttingDown()) {
    return;
  }

  if (!IsWebAppsCrosapiEnabled()) {
    provider_->ui_manager().CloseAppWindows(app_id);
    return;
  }

  CHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));

  apps::BrowserAppInstanceTracker* instance_tracker =
      apps::AppServiceProxyFactory::GetForProfile(profile_)
          ->BrowserAppInstanceTracker();
  CHECK(instance_tracker);

  instance_tracker->StopInstancesOfApp(app_id);
}

void WebAppPublisherHelper::GetCompressedIconData(
    const std::string& app_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  DCHECK(provider_);
  if (IsShuttingDown()) {
    return;
  }

  apps::GetWebAppCompressedIconData(profile_, app_id, size_in_dip, scale_factor,
                                    std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void WebAppPublisherHelper::LoadIcon(const std::string& app_id,
                                     apps::IconType icon_type,
                                     int32_t size_hint_in_dip,
                                     apps::IconEffects icon_effects,
                                     apps::LoadIconCallback callback) {
  DCHECK(provider_);
  if (IsShuttingDown()) {
    return;
  }

  LoadIconFromWebApp(profile_, icon_type, size_hint_in_dip, app_id,
                     icon_effects, std::move(callback));
}

void WebAppPublisherHelper::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::LaunchSource launch_source,
    apps::WindowInfoPtr window_info,
    base::OnceCallback<void(content::WebContents*)> on_complete) {
  if (IsShuttingDown()) {
    std::move(on_complete).Run(nullptr);
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(on_complete).Run(nullptr);
    return;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      web_app->app_id(), event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));

  // The app will be launched for the currently active profile.
  LaunchAppWithParams(std::move(params), std::move(on_complete));
}

void WebAppPublisherHelper::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
  if (IsShuttingDown()) {
    return;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);
  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, launch_source, display::kInvalidDisplayId,
      /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));
  params.launch_files = std::move(file_paths);
  LaunchAppWithFilesCheckingUserPermission(app_id, std::move(params),
                                           base::DoNothing());
}

void WebAppPublisherHelper::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::IntentPtr intent,
    apps::LaunchSource launch_source,
    apps::WindowInfoPtr window_info,
    apps::LaunchCallback callback) {
  CHECK(intent);

  if (IsShuttingDown()) {
    std::move(callback).Run(apps::LaunchResult(apps::State::kFailed));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app_id == guest_os::kTerminalSystemAppId) {
    int64_t display_id =
        window_info ? window_info->display_id : display::kInvalidDisplayId;
    guest_os::LaunchTerminalWithIntent(
        profile_, display_id, std::move(intent),
        base::BindOnce(
            [](apps::LaunchCallback callback, bool success,
               const std::string& failure_reason) {
              if (!success) {
                LOG(WARNING) << "Launch terminal failed: " << failure_reason;
              }
              std::move(callback).Run(apps::ConvertBoolToLaunchResult(success));
            },
            std::move(callback)));
    return;
  }
#endif

  LaunchAppWithIntentImpl(
      app_id, event_flags, std::move(intent), launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      base::BindOnce(
          [](apps::LaunchCallback callback, apps::LaunchSource launch_source,
             std::vector<content::WebContents*> web_contentses) {
// TODO(crbug.com/40184120): Set ArcWebContentsData for Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
            for (content::WebContents* web_contents : web_contentses) {
              if (launch_source == apps::LaunchSource::kFromArc) {
                // Add a flag to remember this tab originated in the ARC
                // context.
                web_contents->SetUserData(
                    &arc::ArcWebContentsData::kArcTransitionFlag,
                    std::make_unique<arc::ArcWebContentsData>(web_contents));
              }
            }
#endif
            std::move(callback).Run(
                apps::ConvertBoolToLaunchResult(!web_contentses.empty()));
          },
          std::move(callback), launch_source));
}

void WebAppPublisherHelper::LaunchAppWithParams(
    apps::AppLaunchParams params,
    base::OnceCallback<void(content::WebContents*)> on_complete) {
  if (IsShuttingDown()) {
    std::move(on_complete).Run(nullptr);
    return;
  }

  apps::AppLaunchParams params_for_restore(
      params.app_id, params.container, params.disposition, params.override_url,
      params.launch_source, params.display_id, params.launch_files,
      params.intent);

  bool is_system_web_app = false;
  std::optional<GURL> override_url = std::nullopt;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Terminal SWA has custom launch code and manages its own restore data.
  if (params.app_id == guest_os::kTerminalSystemAppId) {
    guest_os::LaunchTerminalHome(profile_, params.display_id,
                                 params.restore_id);
    std::move(on_complete).Run(nullptr);
    return;
  }

  auto* swa_manager = ash::SystemWebAppManager::Get(profile());
  if (swa_manager) {
    const WebApp* web_app = GetWebApp(params_for_restore.app_id);
    is_system_web_app = web_app && web_app->IsSystemApp();

    // TODO(crbug.com/40240250): Determine whether override URL can
    // be restored for all SWAs.
    auto system_app_type =
        swa_manager->GetSystemAppTypeForAppId(params_for_restore.app_id);
    if (system_app_type.has_value()) {
      auto* system_app = swa_manager->GetSystemApp(*system_app_type);
      CHECK(system_app);
      if (system_app->ShouldRestoreOverrideUrl()) {
        override_url = params.override_url;
      }
    }
  }

  // Create the FullRestoreSaveHandler instance before launching the app to
  // observe the browser window.
  full_restore::FullRestoreSaveHandler::GetInstance();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  provider_->scheduler().LaunchAppWithCustomParams(
      std::move(params),
      base::BindOnce(&WebAppPublisherHelper::OnLaunchCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(params_for_restore), is_system_web_app,
                     override_url, std::move(on_complete)));
}

void WebAppPublisherHelper::SetPermission(const std::string& app_id,
                                          apps::PermissionPtr permission) {
  if (IsShuttingDown()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  if (permission->permission_type == apps::PermissionType::kFileHandling) {
    if (absl::holds_alternative<bool>(permission->value)) {
      provider_->scheduler().PersistFileHandlersUserChoice(
          app_id, absl::get<bool>(permission->value), base::DoNothing());
    }
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

  DCHECK(absl::holds_alternative<apps::TriState>(permission->value));
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (absl::get<apps::TriState>(permission->value)) {
    case apps::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, permission_value);
}

void WebAppPublisherHelper::OpenNativeSettings(const std::string& app_id) {
  if (IsShuttingDown()) {
    return;
  }

  provider_->ui_manager().ShowWebAppSettings(app_id);
}

apps::WindowMode WebAppPublisherHelper::GetWindowMode(
    const std::string& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return apps::WindowMode::kUnknown;
  }

  auto display_mode = registrar().GetAppEffectiveDisplayMode(web_app->app_id());
  return ConvertDisplayModeToWindowMode(display_mode);
}

void WebAppPublisherHelper::UpdateAppSize(const std::string& app_id) {
  const auto* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  provider_->scheduler().ComputeAppSize(
      app_id, base::BindOnce(&WebAppPublisherHelper::OnGetWebAppSize,
                             weak_ptr_factory_.GetWeakPtr(), app_id));
}

void WebAppPublisherHelper::SetWindowMode(const std::string& app_id,
                                          apps::WindowMode window_mode) {
  auto user_display_mode = mojom::UserDisplayMode::kStandalone;
  switch (window_mode) {
    case apps::WindowMode::kBrowser:
      user_display_mode = mojom::UserDisplayMode::kBrowser;
      break;
    case apps::WindowMode::kUnknown:
    case apps::WindowMode::kWindow:
      user_display_mode = mojom::UserDisplayMode::kStandalone;
      break;
    case apps::WindowMode::kTabbedWindow:
      user_display_mode = mojom::UserDisplayMode::kTabbed;
      break;
  }
  provider_->scheduler().SetUserDisplayMode(app_id, user_display_mode,
                                            base::DoNothing());
}

apps::WindowMode WebAppPublisherHelper::ConvertDisplayModeToWindowMode(
    blink::mojom::DisplayMode display_mode) {
  switch (display_mode) {
    case blink::mojom::DisplayMode::kUndefined:
      return apps::WindowMode::kUnknown;
    case blink::mojom::DisplayMode::kBrowser:
      return apps::WindowMode::kBrowser;
    case blink::mojom::DisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(blink::features::kDesktopPWAsTabStrip) &&
          base::FeatureList::IsEnabled(
              features::kDesktopPWAsTabStripSettings)) {
        return apps::WindowMode::kTabbedWindow;
      } else {
        [[fallthrough]];
      }
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kFullscreen:
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
    case blink::mojom::DisplayMode::kBorderless:
    case blink::mojom::DisplayMode::kPictureInPicture:
      return apps::WindowMode::kWindow;
  }
}

void WebAppPublisherHelper::PublishWindowModeUpdate(
    const std::string& app_id,
    blink::mojom::DisplayMode display_mode) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto app = std::make_unique<apps::App>(app_type(), app_id);
  app->window_mode = ConvertDisplayModeToWindowMode(display_mode);
  delegate_->PublishWebApp(std::move(app));
}

void WebAppPublisherHelper::PublishRunOnOsLoginModeUpdate(
    const std::string& app_id,
    RunOnOsLoginMode run_on_os_login_mode) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto app = std::make_unique<apps::App>(app_type(), app_id);
  const auto login_mode = registrar().GetAppRunOnOsLoginMode(app_id);
  app->run_on_os_login = apps::RunOnOsLogin(
      ConvertOsLoginMode(run_on_os_login_mode), !login_mode.user_controllable);
  delegate_->PublishWebApp(std::move(app));
}

std::string WebAppPublisherHelper::GenerateShortcutId() {
  return base::NumberToString(shortcut_id_generator_.GenerateNextId().value());
}

void WebAppPublisherHelper::StoreShortcutId(
    const std::string& shortcut_id,
    const WebAppShortcutsMenuItemInfo& menu_item_info) {
  shortcut_id_map_.emplace(shortcut_id, std::move(menu_item_info));
}

void WebAppPublisherHelper::ExecuteContextMenuCommand(
    const std::string& app_id,
    const std::string& shortcut_id,
    int64_t display_id,
    base::OnceCallback<void(content::WebContents*)> on_complete) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(on_complete).Run(nullptr);
    return;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params(
      app_id, ConvertDisplayModeToAppLaunchContainer(display_mode),
      WindowOpenDisposition::CURRENT_TAB, apps::LaunchSource::kFromMenu,
      display_id);

  auto menu_item = shortcut_id_map_.find(shortcut_id);
  if (menu_item != shortcut_id_map_.end()) {
    params.override_url = menu_item->second.url;
  }

  LaunchAppWithParams(std::move(params), std::move(on_complete));
}

WebAppRegistrar& WebAppPublisherHelper::registrar() const {
  return provider_->registrar_unsafe();
}

WebAppInstallManager& WebAppPublisherHelper::install_manager() const {
  return provider_->install_manager();
}

bool WebAppPublisherHelper::IsShuttingDown() const {
  return is_shutting_down_;
}

void WebAppPublisherHelper::OnWebAppFileHandlerApprovalStateChanged(
    const webapps::AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    delegate_->PublishWebApp(CreateWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppInstalled(const webapps::AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    auto app = CreateWebApp(web_app);
    // If the installation was a force reinstallation on top of an existing app,
    // the raw icon might have changed. Notify App Service to invalidate the
    // icon disk cache.
    app->icon_key->update_version = true;
    delegate_->PublishWebApp(std::move(app));
  }
}

void WebAppPublisherHelper::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    delegate_->PublishWebApp(CreateWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppManifestUpdated(
    const webapps::AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    auto app = CreateWebApp(web_app);
    // The manifest updated might cause the app raw icon updated. So set
    // a new `raw_icon_data_version`, to remove the icon files saved in the
    // AppService icon directory, to get the new raw icon files of the web app
    // for AppService.
    app->icon_key->update_version = true;
    delegate_->PublishWebApp(std::move(app));
  }
}

void WebAppPublisherHelper::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
#if BUILDFLAG(IS_CHROMEOS)
  // If a web app has been uninstalled, we do not know if it is a shortcut from
  // web app registrar. Here we check if we have got an app registered in
  // AppRegistryCache to be uninstalled. If not, we do not publish the update.
  bool found = apps::AppServiceProxyFactory::GetForProfile(profile_)
                   ->AppRegistryCache()
                   .ForOneApp(app_id, [](const apps::AppUpdate& update) {});
  if (!found) {
    return;
  }

  paused_apps_.MaybeRemoveApp(app_id);

  app_notifications_.RemoveNotificationsForApp(app_id);

  auto result = media_requests_.RemoveRequests(app_id);
  delegate_->ModifyWebAppCapabilityAccess(app_id, result.camera,
                                          result.microphone);
#endif

  delegate_->PublishWebApp(ConvertUninstalledWebApp(app_id, uninstall_source));
}

void WebAppPublisherHelper::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void WebAppPublisherHelper::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void WebAppPublisherHelper::OnWebAppLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  delegate_->PublishWebApp(ConvertLaunchedWebApp(web_app));
}

void WebAppPublisherHelper::OnWebAppUserDisplayModeChanged(
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode) {
  // If the app that changed display mode is not registered in app service, it
  // is because this was considered as a shortcut and now considered as an app
  // due to display mode change, in this case we should publish the full app.
  if (apps::AppServiceProxyFactory::GetForProfile(profile_)
          ->AppRegistryCache()
          .IsAppInstalled(app_id)) {
    PublishWindowModeUpdate(app_id,
                            registrar().GetAppEffectiveDisplayMode(app_id));
  } else {
    const WebApp* web_app = GetWebApp(app_id);
    if (web_app) {
      delegate_->PublishWebApp(CreateWebApp(web_app));
    }
  }
}

void WebAppPublisherHelper::OnWebAppRunOnOsLoginModeChanged(
    const webapps::AppId& app_id,
    RunOnOsLoginMode run_on_os_login_mode) {
  PublishRunOnOsLoginModeUpdate(app_id, run_on_os_login_mode);
}

#if BUILDFLAG(IS_CHROMEOS)
// If is_disabled is set, the app backed by |app_id| is published with readiness
// kDisabledByPolicy, otherwise it's published with readiness kReady.
void WebAppPublisherHelper::OnWebAppDisabledStateChanged(
    const webapps::AppId& app_id,
    bool is_disabled) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  DCHECK_EQ(is_disabled, web_app->chromeos_data()->is_disabled);
  apps::AppPtr app = CreateWebApp(web_app);
  app->icon_key = apps::IconKey(GetIconEffects(web_app));

  // If the disable mode is hidden, update the visibility of the new disabled
  // app.
  if (is_disabled && provider_->policy_manager().IsDisabledAppsModeHidden()) {
    UpdateAppDisabledMode(*app);
  }

  delegate_->PublishWebApp(std::move(app));
}

void WebAppPublisherHelper::OnWebAppsDisabledModeChanged() {
  std::vector<apps::AppPtr> apps;
  std::vector<webapps::AppId> app_ids = registrar().GetAppIds();
  for (const auto& id : app_ids) {
    // We only update visibility of disabled apps in this method. When enabling
    // previously disabled app, OnWebAppDisabledStateChanged() method will be
    // called and this method will update visibility and readiness of the newly
    // enabled app.
    if (provider_->policy_manager().IsWebAppInDisabledList(id)) {
      const WebApp* web_app = GetWebApp(id);
      if (!web_app) {
        continue;
      }
      auto app = std::make_unique<apps::App>(app_type(), web_app->app_id());
      UpdateAppDisabledMode(*app);
      apps.push_back(std::move(app));
    }
  }
  delegate_->PublishWebApps(std::move(apps));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
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
    auto app =
        app_notifications_.CreateAppWithHasBadgeStatus(app_type(), app_id);
    DCHECK(app->has_badge.has_value());
    app->has_badge = ShouldShowBadge(app_id, app->has_badge.value());
    delegate_->PublishWebApp(std::move(app));
  }
}

void WebAppPublisherHelper::OnNotificationDisplayServiceDestroyed(
    NotificationDisplayService* service) {
  DCHECK(notification_display_service_.IsObservingSource(service));
  notification_display_service_.Reset();
}

void WebAppPublisherHelper::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  const webapps::AppId* app_id = WebAppTabHelper::GetAppId(web_contents);
  if (!app_id) {
    return;
  }
  auto result = media_requests_.UpdateCameraState(*app_id, web_contents,
                                                  is_capturing_video);
  delegate_->ModifyWebAppCapabilityAccess(*app_id, result.camera,
                                          result.microphone);
}

void WebAppPublisherHelper::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  const webapps::AppId* app_id = WebAppTabHelper::GetAppId(web_contents);
  if (!app_id) {
    return;
  }
  auto result = media_requests_.UpdateMicrophoneState(*app_id, web_contents,
                                                      is_capturing_audio);
  delegate_->ModifyWebAppCapabilityAccess(*app_id, result.camera,
                                          result.microphone);
}
#endif

void WebAppPublisherHelper::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  DCHECK(!IsShuttingDown());
  // If content_type is not one of the supported permissions, do nothing.
  if (!content_type_set.ContainsAllTypes() &&
      !IsSupportedWebAppPermissionType(content_type_set.GetType())) {
    return;
  }

  for (const WebApp& web_app : registrar().GetApps()) {
    if (primary_pattern.Matches(web_app.start_url())) {
      auto app = std::make_unique<apps::App>(app_type(), web_app.app_id());
      app->permissions = CreatePermissions(&web_app);
      delegate_->PublishWebApp(std::move(app));
    }
  }
}

void WebAppPublisherHelper::OnWebAppSettingsPolicyChanged() {
  DCHECK(!IsShuttingDown());

  for (const WebApp& web_app : registrar().GetApps()) {
    delegate_->PublishWebApp(CreateWebApp(&web_app));
  }
}

void WebAppPublisherHelper::Init() {
  // Allow for web app migration tests.
  // In some tests, WebAppPublisherHelper could be created during the shutdown
  // stage as the web app publisher is created async by AppServiceProxy. So
  // provider_ could be null in some tests.
  if (!AreWebAppsEnabled(profile_) || !provider_) {
    return;
  }

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppPublisherHelper::ObserveWebAppSubsystems,
                                weak_ptr_factory_.GetWeakPtr()));

  content_settings_observation_.Observe(
      HostContentSettingsMapFactory::GetForProfile(profile_));

#if BUILDFLAG(IS_CHROMEOS)
  // NotificationDisplayService could be null in some tests.
  if (auto* notification_display_service =
          NotificationDisplayServiceFactory::GetForProfile(profile())) {
    notification_display_service_.Observe(notification_display_service);
  }

  badge_manager_ = badging::BadgeManagerFactory::GetForProfile(profile());
  // badge_manager_ is nullptr in guest and incognito profiles.
  if (badge_manager_) {
    badge_manager_->SetDelegate(
        std::make_unique<WebAppPublisherHelper::BadgeManagerDelegate>(
            weak_ptr_factory_.GetWeakPtr()));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  media_indicator_observation_.Observe(
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get());
#endif
}

void WebAppPublisherHelper::ObserveWebAppSubsystems() {
  install_manager_observation_.Observe(&install_manager());
  registrar_observation_.Observe(&registrar());
}

IconEffects WebAppPublisherHelper::GetIconEffects(const WebApp* web_app) {
  IconEffects icon_effects = IconEffects::kRoundCorners;
  if (web_app->install_state() ==
      proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE) {
    icon_effects |= IconEffects::kBlocked;
  }

  icon_effects |= web_app->is_generated_icon() ? IconEffects::kCrOsStandardMask
                                               : IconEffects::kCrOsStandardIcon;

#if BUILDFLAG(IS_CHROMEOS)
  if (IsPaused(web_app->app_id())) {
    icon_effects |= IconEffects::kPaused;
  }
#endif

  bool is_disabled = false;
  if (web_app->chromeos_data().has_value()) {
    is_disabled = web_app->chromeos_data()->is_disabled;
  }
  if (is_disabled) {
    icon_effects |= IconEffects::kBlocked;
  }

  return icon_effects;
}

const WebApp* WebAppPublisherHelper::GetWebApp(
    const webapps::AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

void WebAppPublisherHelper::LaunchAppWithIntentImpl(
    const std::string& app_id,
    int32_t event_flags,
    apps::IntentPtr intent,
    apps::LaunchSource launch_source,
    int64_t display_id,
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback) {
  bool is_file_handling_launch =
      intent && !intent->files.empty() && !intent->IsShareIntent();
  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, event_flags, launch_source, display_id,
      ConvertDisplayModeToAppLaunchContainer(
          registrar().GetAppEffectiveDisplayMode(app_id)),
      std::move(intent), profile_);
  if (is_file_handling_launch) {
    LaunchAppWithFilesCheckingUserPermission(app_id, std::move(params),
                                             std::move(callback));
    return;
  }

  LaunchAppWithParams(
      std::move(params),
      base::BindOnce(
          [](base::OnceCallback<void(std::vector<content::WebContents*>)>
                 callback,
             content::WebContents* contents) {
            // These calls are piped through LaunchWebAppCommand and can end
            // early during an Abort due to various reasons (like
            // FirstRunService not completed), in which case there will be no
            // web contents.
            if (contents) {
              std::move(callback).Run({contents});
            } else {
              std::move(callback).Run({});
            }
          },
          std::move(callback)));
}

std::vector<std::string> WebAppPublisherHelper::GetPolicyIds(
    const WebApp& web_app) const {
  const auto& app_id = web_app.app_id();

  if (web_app.isolation_data() && registrar().IsInstalledByPolicy(app_id)) {
    // This is an IWA - and thus, web_bundle_id == policy_id == URL hostname
    return {web_app.start_url().host()};
  }

  std::vector<std::string> policy_ids;

  if (std::optional<std::string_view> preinstalled_web_app_policy_id =
          apps_util::GetPolicyIdForPreinstalledWebApp(app_id)) {
    policy_ids.emplace_back(*preinstalled_web_app_policy_id);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* swa_manager = ash::SystemWebAppManager::Get(profile());
  if (swa_manager && swa_manager->IsSystemWebApp(app_id)) {
    const auto& swa_data = web_app.client_data().system_web_app_data;
    DCHECK(swa_data);
    const ash::SystemWebAppType swa_type = swa_data->system_app_type;
    const std::optional<std::string_view> swa_policy_id =
        apps_util::GetPolicyIdForSystemWebAppType(swa_type);
    if (swa_policy_id) {
      policy_ids.emplace_back(*swa_policy_id);
    }

    // File Manager SWA uses File Manager Extension's ID for policy.
    if (swa_type == ash::SystemWebAppType::FILE_MANAGER) {
      policy_ids.push_back(file_manager::kFileManagerAppId);
    }
  }
#endif  // BUIDLFLAG(IS_CHROMEOS_ASH)

  for (const auto& [source, external_config] :
       web_app.management_to_external_config_map()) {
    if (!external_config.additional_policy_ids.empty()) {
      base::ranges::copy(external_config.additional_policy_ids,
                         std::back_inserter(policy_ids));
    }
  }

  if (!registrar().HasExternalAppWithInstallSource(
          app_id, ExternalInstallSource::kExternalPolicy)) {
    return policy_ids;
  }

  base::flat_map<webapps::AppId, base::flat_set<GURL>> installed_apps =
      registrar().GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  if (auto* install_urls = base::FindOrNull(installed_apps, app_id)) {
    DCHECK(!install_urls->empty());
    base::Extend(policy_ids, base::ToVector(*install_urls, &GURL::spec));
  }

  return policy_ids;
}

apps::PackageId WebAppPublisherHelper::GetPackageId(
    const WebApp& web_app) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (web_app.client_data().system_web_app_data) {
    const std::optional<std::string_view> policy_id =
        apps_util::GetPolicyIdForSystemWebAppType(
            web_app.client_data().system_web_app_data->system_app_type);
    if (policy_id) {
      return apps::PackageId(apps::PackageType::kSystem, *policy_id);
    }
  }
#endif
  return apps::PackageId(apps::PackageType::kWeb, web_app.manifest_id().spec());
}

#if BUILDFLAG(IS_CHROMEOS)
void WebAppPublisherHelper::UpdateAppDisabledMode(apps::App& app) {
  if (provider_->policy_manager().IsDisabledAppsModeHidden()) {
    app.show_in_launcher = false;
    app.show_in_search = false;
    app.show_in_shelf = false;
    return;
  }
  app.show_in_launcher = true;
  app.show_in_search = true;
  app.show_in_shelf = true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* swa_manager = ash::SystemWebAppManager::Get(profile());
  if (!swa_manager) {
    return;
  }
  auto system_app_type = swa_manager->GetSystemAppTypeForAppId(app.app_id);
  if (system_app_type.has_value()) {
    auto* system_app = swa_manager->GetSystemApp(*system_app_type);
    DCHECK(system_app);
    app.show_in_launcher = system_app->ShouldShowInLauncher();
    app.show_in_shelf = system_app->ShouldShowInSearchAndShelf();
    app.show_in_search = system_app->ShouldShowInSearchAndShelf();
  }
#endif
}

bool WebAppPublisherHelper::MaybeAddNotification(
    const std::string& app_id,
    const std::string& notification_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return false;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  auto app = app_notifications_.CreateAppWithHasBadgeStatus(app_type(), app_id);
  DCHECK(app->has_badge.has_value());
  app->has_badge = ShouldShowBadge(app_id, app->has_badge.value());
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
    std::optional<webapps::AppId> app_id = FindInstalledAppWithUrlInScope(
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
    for (const auto& app_id : app_ids) {
      MaybeAddNotification(app_id, notification.id());
    }
  }
}

bool WebAppPublisherHelper::ShouldShowBadge(const std::string& app_id,
                                            bool has_notification) {
  // We show a badge if either the Web Badging API recently has a badge set, or
  // the Badging API has not been recently used by the app and a notification is
  // showing.
  if (!badge_manager_ || !badge_manager_->HasRecentApiUsage(app_id)) {
    return has_notification;
  }

  return badge_manager_->GetBadgeValue(app_id).has_value();
}
#endif

void WebAppPublisherHelper::LaunchAppWithFilesCheckingUserPermission(
    const std::string& app_id,
    apps::AppLaunchParams params,
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback) {
  std::vector<base::FilePath> file_paths = params.launch_files;
  auto launch_callback =
      base::BindOnce(&WebAppPublisherHelper::OnFileHandlerDialogCompleted,
                     weak_ptr_factory_.GetWeakPtr(), app_id, std::move(params),
                     std::move(callback));

  switch (
      provider_->registrar_unsafe().GetAppFileHandlerApprovalState(app_id)) {
    case ApiApprovalState::kRequiresPrompt:
      provider_->ui_manager().ShowWebAppFileLaunchDialog(
          file_paths, app_id, std::move(launch_callback));
      break;
    case ApiApprovalState::kAllowed:
      std::move(launch_callback)
          .Run(/*allowed=*/true, /*remember_user_choice=*/false);
      break;
    case ApiApprovalState::kDisallowed:
      // We shouldn't have gotten this far (i.e. "open with" should not have
      // been selectable) if file handling was already disallowed for the app.
      NOTREACHED_IN_MIGRATION();
      std::move(launch_callback)
          .Run(/*allowed=*/false, /*remember_user_choice=*/false);
      break;
  }
}

void WebAppPublisherHelper::OnFileHandlerDialogCompleted(
    std::string app_id,
    apps::AppLaunchParams params,
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback,
    bool allowed,
    bool remember_user_choice) {
  if (remember_user_choice) {
    provider_->scheduler().PersistFileHandlersUserChoice(app_id, allowed,
                                                         base::DoNothing());
  }

  if (!allowed) {
    std::move(callback).Run({});
    return;
  }

  // System web apps behave differently than when launching a normal PWA with
  // the File Handling API. Per the web spec, PWAs require that the extension
  // matches what's specified in the manifest. System apps rely on MIME type
  // sniffing to work even when the extensions don't match. For this reason,
  // `GetMatchingFileHandlerUrls` and therefore multilaunch won't work for
  // system apps.
  const WebApp* web_app = GetWebApp(params.app_id);
  bool can_multilaunch = !(web_app && web_app->IsSystemApp());
  base::ConcurrentCallbacks<content::WebContents*> concurrent;

  if (can_multilaunch) {
    WebAppFileHandlerManager::LaunchInfos file_launch_infos =
        provider_->os_integration_manager()
            .file_handler_manager()
            .GetMatchingFileHandlerUrls(app_id, params.launch_files);
    for (const auto& [url, files] : file_launch_infos) {
      apps::AppLaunchParams params_for_file_launch(
          app_id, params.container, params.disposition, params.launch_source,
          params.display_id, files, nullptr);
      params_for_file_launch.override_url = url;
      LaunchAppWithParams(std::move(params_for_file_launch),
                          concurrent.CreateCallback());
    }
  } else {
    apps::AppLaunchParams params_for_file_launch(
        app_id, params.container, params.disposition, params.launch_source,
        params.display_id, params.launch_files, params.intent);
    // For system web apps, the URL is calculated by the file browser and passed
    // in the intent.
    // TODO(crbug.com/40203246): remove this check. It's only here to support
    // tests that haven't been updated.
    if (params.intent) {
      params_for_file_launch.override_url = GURL(*params.intent->activity_name);
    }
    LaunchAppWithParams(std::move(params_for_file_launch),
                        concurrent.CreateCallback());
  }

  std::move(concurrent).Done(std::move(callback));
}

void WebAppPublisherHelper::OnLaunchCompleted(
    apps::AppLaunchParams params_for_restore,
    bool is_system_web_app,
    std::optional<GURL> override_url,
    base::OnceCallback<void(content::WebContents*)> on_complete,
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Save all launch information for system web apps, because the
  // browser session restore can't restore system web apps.
  int session_id =
      apps::GetSessionIdForRestoreFromWebContents(web_contents.get());
  if (SessionID::IsValidValue(session_id)) {
    if (is_system_web_app) {
      std::unique_ptr<app_restore::AppLaunchInfo> launch_info =
          std::make_unique<app_restore::AppLaunchInfo>(
              params_for_restore.app_id, session_id,
              params_for_restore.container, params_for_restore.disposition,
              params_for_restore.display_id,
              std::move(params_for_restore.launch_files),
              std::move(params_for_restore.intent));

      if (override_url) {
        launch_info->override_url = override_url.value();
      }

      full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                      std::move(launch_info));
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::move(on_complete).Run(web_contents.get());
}

void WebAppPublisherHelper::OnGetWebAppSize(
    webapps::AppId app_id,
    std::optional<ComputedAppSize> size) {
  auto app = std::make_unique<apps::App>(app_type(), app_id);
  if (!size.has_value()) {
    return;
  }
  app->app_size_in_bytes = size->app_size_in_bytes;
  app->data_size_in_bytes = size->data_size_in_bytes;
  delegate_->PublishWebApp(std::move(app));
}

}  // namespace web_app
