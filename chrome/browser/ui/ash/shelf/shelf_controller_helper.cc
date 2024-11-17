// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"

#include <optional>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/shortcut_metrics.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
#include "chrome/browser/apps/browser_instance/web_contents_instance_id_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr float kProgressNone = 0;
constexpr float kProgressNotApplicable = -1;

std::string GetSourceFromAppListSource(ash::ShelfLaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_APP_LIST:
      return std::string(extension_urls::kLaunchSourceAppList);
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return std::string(extension_urls::kLaunchSourceAppListSearch);
    default:
      return std::string();
  }
}

}  // namespace

ShelfControllerHelper::ShelfControllerHelper(Profile* profile)
    : profile_(profile) {}

ShelfControllerHelper::~ShelfControllerHelper() {}

std::u16string ShelfControllerHelper::GetLabelForPromiseStatus(
    apps::PromiseStatus status) {
  switch (status) {
    case apps::PromiseStatus::kUnknown:
    case apps::PromiseStatus::kPending:
      return l10n_util::GetStringUTF16(IDS_PROMISE_STATUS_WAITING);
    case apps::PromiseStatus::kInstalling:
    case apps::PromiseStatus::kSuccess:
    case apps::PromiseStatus::kCancelled:
      return l10n_util::GetStringUTF16(IDS_PROMISE_STATUS_INSTALLING);
  }
}

std::u16string ShelfControllerHelper::GetAccessibleLabelForPromiseStatus(
    std::optional<std::string> name,
    apps::PromiseStatus status) {
  switch (status) {
    case apps::PromiseStatus::kUnknown:
    case apps::PromiseStatus::kPending:
      if (!name.has_value()) {
        return l10n_util::GetStringUTF16(
            IDS_PROMISE_APP_PLACEHOLDER_ACCESSIBLE_LABEL_WAITING);
      }
      return l10n_util::GetStringFUTF16(
          IDS_PROMISE_APP_ACCESSIBLE_LABEL_WAITING,
          {base::UTF8ToUTF16(name.value())});
    case apps::PromiseStatus::kInstalling:
    case apps::PromiseStatus::kSuccess:
    case apps::PromiseStatus::kCancelled:
      if (!name.has_value()) {
        return l10n_util::GetStringUTF16(
            IDS_PROMISE_APP_PLACEHOLDER_ACCESSIBLE_LABEL_INSTALLING);
      }
      return l10n_util::GetStringFUTF16(
          IDS_PROMISE_APP_ACCESSIBLE_LABEL_INSTALLING,
          {base::UTF8ToUTF16(name.value())});
  }
}

// static
std::u16string ShelfControllerHelper::GetAppTitle(Profile* profile,
                                                  const std::string& app_id) {
  if (app_id.empty())
    return std::u16string();

  // Get the title if the app is an ARC app. ARC shortcuts could call this
  // function when it's created, so AppService can't be used for ARC shortcuts,
  // because AppService is async.
  if (arc::IsArcItem(profile, app_id)) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        ArcAppListPrefs::Get(profile)->GetApp(
            arc::ArcAppShelfId::FromString(app_id).app_id());
    DCHECK(app_info.get());
    return base::UTF8ToUTF16(app_info->name);
  }

  std::string name;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&name](const apps::AppUpdate& update) {
        if (apps_util::IsInstalled(update.Readiness())) {
          name = update.Name();
        }
      });
  if (!name.empty()) {
    return base::UTF8ToUTF16(name);
  }

  if (ash::features::ArePromiseIconsEnabled()) {
    const std::u16string promise_app_title =
        GetPromiseAppTitle(profile, app_id);
    if (!promise_app_title.empty()) {
      return promise_app_title;
    }
  }

  // Get the title for the extension which is not managed by AppService.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!registry)
    return std::u16string();

  auto* extension = registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (extension && extension->is_extension())
    return base::UTF8ToUTF16(extension->name());

  return std::u16string();
}

std::u16string ShelfControllerHelper::GetPromiseAppAccessibleName(
    Profile* profile,
    const std::string& package_id) {
  if (!ash::features::ArePromiseIconsEnabled()) {
    return std::u16string();
  }
  const apps::PromiseApp* promise_app =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->PromiseAppRegistryCache()
          ->GetPromiseAppForStringPackageId(package_id);
  if (!promise_app) {
    return std::u16string();
  }
  return GetAccessibleLabelForPromiseStatus(promise_app->name,
                                            promise_app->status);
}

// static
std::string ShelfControllerHelper::GetAppPackageId(Profile* profile,
                                                   const std::string& app_id) {
  if (app_id.empty()) {
    return std::string();
  }

  if (ash::features::ArePromiseIconsEnabled()) {
    const apps::PromiseApp* promise_app =
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->PromiseAppRegistryCache()
            ->GetPromiseAppForStringPackageId(app_id);
    if (promise_app) {
      return promise_app->package_id.ToString();
    }
  }

  std::optional<apps::PackageId> package_id;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&package_id, profile](const apps::AppUpdate& update) {
        if (apps_util::IsInstalled(update.Readiness())) {
          package_id = apps_util::GetPackageIdForApp(profile, update);
        }
      });
  if (package_id) {
    return package_id->ToString();
  }

  return std::string();
}

// static
ash::AppStatus ShelfControllerHelper::GetAppStatus(Profile* profile,
                                                   const std::string& app_id) {
  ash::AppStatus status = ash::AppStatus::kReady;

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return status;

  if (ash::features::ArePromiseIconsEnabled()) {
    const apps::PromiseApp* promise_app =
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->PromiseAppRegistryCache()
            ->GetPromiseAppForStringPackageId(app_id);
    if (promise_app) {
      return ConvertPromiseStatusToAppStatus(promise_app->status);
    }
  }

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&status](const apps::AppUpdate& update) {
        if (apps_util::IsDisabled(update.Readiness())) {
          status = ash::AppStatus::kBlocked;
        } else if (update.Paused().value_or(false)) {
          status = ash::AppStatus::kPaused;
        }
      });

  return status;
}

// static
bool ShelfControllerHelper::IsAppDefaultInstalled(Profile* profile,
                                                  const std::string& app_id) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  bool default_installed = false;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&default_installed](const apps::AppUpdate& update) {
        default_installed =
            update.InstallReason() == apps::InstallReason::kDefault ||
            update.InstallReason() == apps::InstallReason::kSystem;
      });
  return default_installed;
}

std::string ShelfControllerHelper::GetAppID(content::WebContents* tab) {
  DCHECK(tab);
  return apps::GetInstanceAppIdForWebContents(tab).value_or(std::string());
}

std::u16string ShelfControllerHelper::GetPromiseAppTitle(
    Profile* profile,
    const std::string& string_package_id) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return std::u16string();
  }

  const apps::PromiseApp* promise_app =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->PromiseAppRegistryCache()
          ->GetPromiseAppForStringPackageId(string_package_id);
  if (!promise_app) {
    return std::u16string();
  }

  return GetLabelForPromiseStatus(promise_app->status);
}

// static
float ShelfControllerHelper::GetPromiseAppProgress(
    Profile* profile,
    const std::string& string_package_id) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return kProgressNotApplicable;
  }
  const apps::PromiseApp* promise_app =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->PromiseAppRegistryCache()
          ->GetPromiseAppForStringPackageId(string_package_id);
  if (!promise_app) {
    return kProgressNotApplicable;
  }
  if (!promise_app->progress.has_value()) {
    return kProgressNone;
  }
  return promise_app->progress.value();
}

// static
bool ShelfControllerHelper::IsPromiseApp(Profile* profile,
                                         const std::string& id) {
  if (!ash::features::ArePromiseIconsEnabled()) {
    return false;
  }
  return apps::AppServiceProxyFactory::GetForProfile(profile)
      ->PromiseAppRegistryCache()
      ->GetPromiseAppForStringPackageId(id);
}

// static
ash::AppStatus ShelfControllerHelper::ConvertPromiseStatusToAppStatus(
    apps::PromiseStatus promise_status) {
  switch (promise_status) {
    case apps::PromiseStatus::kUnknown:
      // Fallthrough.
    case apps::PromiseStatus::kPending:
      return ash::AppStatus::kPending;
    case apps::PromiseStatus::kInstalling:
      return ash::AppStatus::kInstalling;
    case apps::PromiseStatus::kSuccess:
      return ash::AppStatus::kInstallSuccess;
    case apps::PromiseStatus::kCancelled:
      return ash::AppStatus::kInstallCancelled;
  }
}

bool ShelfControllerHelper::IsValidIDForCurrentUser(
    const std::string& app_id) const {
  if (IsValidIDForArcApp(app_id))
    return true;

  return IsValidIDFromAppService(app_id);
}

void ShelfControllerHelper::LaunchApp(const ash::ShelfID& id,
                                      ash::ShelfLaunchSource source,
                                      int event_flags,
                                      int64_t display_id,
                                      bool new_window) {
  const std::string& app_id = id.app_id;
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // Launch apps with AppServiceProxy.Launch.
  if (proxy->AppRegistryCache().GetAppType(app_id) != apps::AppType::kUnknown) {
    if (new_window) {
      apps::LaunchContainer container =
          apps::LaunchContainer::kLaunchContainerNone;
      proxy->AppRegistryCache().ForOneApp(
          app_id, [&](const apps::AppUpdate& update) {
            container = apps::ConvertWindowModeToAppLaunchContainer(
                update.WindowMode());
          });
      // TODO(b/310775293): Make this CHECK(launches_in_window), currently this
      // could be false due to the ash::LAUNCH_NEW shelf command being used for
      // both "New window" and "New tab".
      switch (container) {
        case apps::LaunchContainer::kLaunchContainerWindow:
          // TODO(b/310775293): Add test for this behaviour.
          // event_flags are ignored here because they control the disposition
          // which in this case has been overridden to always open a new window.
          proxy->LaunchAppWithParams(apps::AppLaunchParams(
              app_id, container, WindowOpenDisposition::NEW_WINDOW,
              ShelfLaunchSourceToAppsLaunchSource(source), display_id));
          return;
        case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
        case apps::LaunchContainer::kLaunchContainerTab:
        case apps::LaunchContainer::kLaunchContainerNone:
          break;
      }
    }
    proxy->Launch(app_id, event_flags,
                  ShelfLaunchSourceToAppsLaunchSource(source),
                  std::make_unique<apps::WindowInfo>(display_id));
    return;
  }

  // Handle user selects promise app from Shelf
  if (IsPromiseApp(profile_, app_id)) {
    proxy->PromiseAppService()->UpdateInstallPriority(app_id);
    return;
  }

  // For extensions, Launch with AppServiceProxy.LaunchAppWithParams.

  // |extension| could be null when it is being unloaded for updating.
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return;

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    // Do nothing if there is already a running enable flow.
    if (extension_enable_flow_)
      return;

    extension_enable_flow_ =
        std::make_unique<ExtensionEnableFlow>(profile_, app_id, this);
    extension_enable_flow_->StartForNativeWindow(nullptr);
    return;
  }

  apps::AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
      profile_, extension, event_flags,
      ShelfLaunchSourceToAppsLaunchSource(source), display_id);
  if ((source == ash::LAUNCH_FROM_APP_LIST ||
       source == ash::LAUNCH_FROM_APP_LIST_SEARCH) &&
      app_id == extensions::kWebStoreAppId) {
    // Get the corresponding source string.
    std::string source_value = GetSourceFromAppListSource(source);

    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url, extension_urls::kWebstoreSourceField, source_value);
  }
  params.launch_id = id.launch_id;

  ::OpenApplication(profile_, std::move(params));
}

ArcAppListPrefs* ShelfControllerHelper::GetArcAppListPrefs() const {
  return ArcAppListPrefs::Get(profile_);
}

void ShelfControllerHelper::ExtensionEnableFlowFinished() {
  LaunchApp(ash::ShelfID(extension_enable_flow_->extension_id()),
            ash::LAUNCH_FROM_UNKNOWN, ui::EF_NONE, display::kInvalidDisplayId,
            /*new_window=*/false);
  extension_enable_flow_.reset();
}

void ShelfControllerHelper::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
}

bool ShelfControllerHelper::IsValidIDForArcApp(
    const std::string& app_id) const {
  const ArcAppListPrefs* arc_prefs = GetArcAppListPrefs();
  if (arc_prefs && arc_prefs->IsRegistered(app_id)) {
    return true;
  }

  if (app_id == arc::kPlayStoreAppId) {
    if (!arc::IsArcAllowedForProfile(profile()) ||
        !arc::IsPlayStoreAvailable()) {
      return false;
    }
    const arc::ArcSessionManager* arc_session_manager =
        arc::ArcSessionManager::Get();
    DCHECK(arc_session_manager);
    if (!arc_session_manager->IsAllowed()) {
      return false;
    }
    if (!arc::IsArcPlayStoreEnabledForProfile(profile()) &&
        arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile())) {
      return false;
    }
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    if (!registry->GetInstalledExtension(arc::kPlayStoreAppId)) {
      return false;
    }
    return true;
  }

  return false;
}

bool ShelfControllerHelper::IsValidIDFromAppService(
    const std::string& app_id) const {
  if (guest_os::IsUnregisteredCrostiniShelfAppId(app_id)) {
    return true;
  }

  bool is_valid = false;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_valid](const apps::AppUpdate& update) {
        if (update.AppType() != apps::AppType::kArc &&
            update.AppType() != apps::AppType::kUnknown &&
            update.Readiness() != apps::Readiness::kUnknown &&
            apps_util::IsInstalled(update.Readiness())) {
          is_valid = true;
        }
      });

  return is_valid;
}
