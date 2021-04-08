// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "net/base/url_util.h"

namespace {

const extensions::Extension* GetExtensionForTab(Profile* profile,
                                                content::WebContents* tab) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service || !extension_service->extensions_enabled())
    return nullptr;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  const GURL url = tab->GetURL();
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  const extensions::Extension* extension = extensions.GetAppByURL(url);
  if (extension && !extensions::LaunchesInWindow(profile, extension))
    return extension;

  // Bookmark app windows should match their launch url extension despite
  // their web extents.
  for (const auto& i : extensions) {
    if (i.get()->from_bookmark() &&
        extensions::IsInNavigationScopeForLaunchUrl(
            extensions::AppLaunchInfo::GetLaunchWebURL(i.get()), url) &&
        !extensions::LaunchesInWindow(profile, i.get())) {
      return i.get();
    }
  }
  return nullptr;
}

base::Optional<std::string> GetAppIdForTab(Profile* profile,
                                           content::WebContents* tab) {
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    if (web_app::WebAppProviderBase* provider =
            web_app::WebAppProviderBase::GetProviderBase(profile)) {
      // Use the Browser's app name to determine the web app for app windows and
      // use the tab's url for app tabs.

      // Note: It is possible to come here after a tab got removed from the
      // browser before it gets destroyed, in which case there is no browser.
      if (Browser* browser = chrome::FindBrowserWithWebContents(tab)) {
        if (browser->app_controller() && browser->app_controller()->HasAppId())
          return browser->app_controller()->GetAppId();
      }

      base::Optional<web_app::AppId> app_id =
          provider->registrar().FindAppWithUrlInScope(tab->GetURL());
      if (app_id && provider->registrar().GetAppUserDisplayMode(*app_id) ==
                        web_app::DisplayMode::kBrowser) {
        return app_id;
      }
    }
  }

  // Note: It is possible to come here after a tab got removed form the browser
  // before it gets destroyed, in which case there is no browser.
  Browser* browser = chrome::FindBrowserWithWebContents(tab);

  // Use the Browser's app name.
  if (browser && browser->deprecated_is_app())
    return web_app::GetAppIdFromApplicationName(browser->app_name());

  const extensions::Extension* extension = GetExtensionForTab(profile, tab);
  if (extension &&
      (!extension->from_bookmark() ||
       !base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)))
    return extension->id();
  return base::nullopt;
}

apps::mojom::LaunchSource ConvertLaunchSource(ash::ShelfLaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_UNKNOWN:
      return apps::mojom::LaunchSource::kUnknown;
    case ash::LAUNCH_FROM_APP_LIST:
      return apps::mojom::LaunchSource::kFromAppListGrid;
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return apps::mojom::LaunchSource::kFromAppListQuery;
    case ash::LAUNCH_FROM_SHELF:
      return apps::mojom::LaunchSource::kFromShelf;
  }
}

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

LauncherControllerHelper::LauncherControllerHelper(Profile* profile)
    : profile_(profile) {}

LauncherControllerHelper::~LauncherControllerHelper() {}

// static
std::u16string LauncherControllerHelper::GetAppTitle(
    Profile* profile,
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
        name = update.Name();
      });
  if (!name.empty())
    return base::UTF8ToUTF16(name);

  // Get the title for the extension which is not managed by AppService.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!registry)
    return std::u16string();

  auto* extension = registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (extension && extension->is_extension())
    return base::UTF8ToUTF16(extension->name());

  if (crostini::IsUnmatchedCrostiniShelfAppId(app_id))
    return crostini::GetCrostiniShelfTitle(app_id);

  return std::u16string();
}

// static
ash::AppStatus LauncherControllerHelper::GetAppStatus(
    Profile* profile,
    const std::string& app_id) {
  ash::AppStatus status = ash::AppStatus::kReady;

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return status;

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&status](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kDisabledByPolicy)
          status = ash::AppStatus::kBlocked;
        else if (update.Paused() == apps::mojom::OptionalBool::kTrue)
          status = ash::AppStatus::kPaused;
      });

  return status;
}

std::string LauncherControllerHelper::GetAppID(content::WebContents* tab) {
  DCHECK(tab);
  base::Optional<std::string> app_id = GetAppIdForTab(
      Profile::FromBrowserContext(tab->GetBrowserContext()), tab);
  return app_id.value_or(std::string());
}

bool LauncherControllerHelper::IsValidIDForCurrentUser(
    const std::string& app_id) const {
  if (IsValidIDForArcApp(app_id))
    return true;

  return IsValidIDFromAppService(app_id);
}

void LauncherControllerHelper::LaunchApp(const ash::ShelfID& id,
                                         ash::ShelfLaunchSource source,
                                         int event_flags,
                                         int64_t display_id) {
  // Handle recording app launch source from the Shelf in Demo Mode.
  if (source == ash::ShelfLaunchSource::LAUNCH_FROM_SHELF) {
    chromeos::DemoSession::RecordAppLaunchSourceIfInDemoMode(
        chromeos::DemoSession::AppLaunchSource::kShelf);
  }

  const std::string& app_id = id.app_id;
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // Launch apps with AppServiceProxy.Launch.
  if (proxy->AppRegistryCache().GetAppType(app_id) !=
      apps::mojom::AppType::kUnknown) {
    proxy->Launch(app_id, event_flags, ConvertLaunchSource(source),
                  apps::MakeWindowInfo(display_id));
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
      apps::mojom::AppLaunchSource::kSourceAppLauncher, display_id);
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

  proxy->BrowserAppLauncher()->LaunchAppWithParams(std::move(params));
}

ArcAppListPrefs* LauncherControllerHelper::GetArcAppListPrefs() const {
  return ArcAppListPrefs::Get(profile_);
}

void LauncherControllerHelper::ExtensionEnableFlowFinished() {
  LaunchApp(ash::ShelfID(extension_enable_flow_->extension_id()),
            ash::LAUNCH_FROM_UNKNOWN, ui::EF_NONE, display::kInvalidDisplayId);
  extension_enable_flow_.reset();
}

void LauncherControllerHelper::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
}

bool LauncherControllerHelper::IsValidIDForArcApp(
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
    return true;
  }

  return false;
}

bool LauncherControllerHelper::IsValidIDFromAppService(
    const std::string& app_id) const {
  if (crostini::IsUnmatchedCrostiniShelfAppId(app_id)) {
    return true;
  }

  bool is_valid = false;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_valid](const apps::AppUpdate& update) {
        if (update.AppType() != apps::mojom::AppType::kArc &&
            update.AppType() != apps::mojom::AppType::kUnknown &&
            update.Readiness() != apps::mojom::Readiness::kUnknown &&
            update.Readiness() != apps::mojom::Readiness::kUninstalledByUser) {
          is_valid = true;
        }
      });

  return is_valid;
}
