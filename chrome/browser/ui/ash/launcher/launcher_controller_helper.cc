// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
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
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/base/url_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"

namespace {

const extensions::Extension* GetExtensionForTab(Profile* profile,
                                                content::WebContents* tab) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service || !extension_service->extensions_enabled())
    return nullptr;

  // Note: It is possible to come here after a tab got removed form the browser
  // before it gets destroyed, in which case there is no browser.
  Browser* browser = chrome::FindBrowserWithWebContents(tab);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  // Use the Browser's app name to determine the extension for app windows and
  // use the tab's url for app tabs.
  if (browser && browser->is_app()) {
    return registry->GetExtensionById(
        web_app::GetAppIdFromApplicationName(browser->app_name()),
        extensions::ExtensionRegistry::EVERYTHING);
  }

  const GURL url = tab->GetURL();
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  const extensions::Extension* extension = extensions.GetAppByURL(url);
  if (extension && !extensions::LaunchesInWindow(profile, extension))
    return extension;

  // Bookmark app windows should match their launch url extension despite
  // their web extents.
  if (extensions::util::IsNewBookmarkAppsEnabled()) {
    for (const auto& i : extensions) {
      if (i.get()->from_bookmark() &&
          extensions::AppLaunchInfo::GetLaunchWebURL(i.get()) == url &&
          !extensions::LaunchesInWindow(profile, i.get())) {
        return i.get();
      }
    }
  }
  return nullptr;
}

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      id, extensions::ExtensionRegistry::EVERYTHING);
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
base::string16 LauncherControllerHelper::GetAppTitle(
    Profile* profile,
    const std::string& app_id) {
  if (app_id.empty())
    return base::string16();

  // Get the title if the app is an ARC app.
  if (arc::IsArcItem(profile, app_id)) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        ArcAppListPrefs::Get(profile)->GetApp(
            arc::ArcAppShelfId::FromString(app_id).app_id());
    DCHECK(app_info.get());
    if (app_info)
      return base::UTF8ToUTF16(app_info->name);
  }

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  if (registry_service && registry_service->IsCrostiniShelfAppId(app_id)) {
    base::Optional<crostini::CrostiniRegistryService::Registration>
        registration = registry_service->GetRegistration(app_id);
    if (!registration)
      return base::string16();
    return base::UTF8ToUTF16(registration->Name());
  }

  const extensions::Extension* extension = GetExtensionByID(profile, app_id);
  if (extension)
    return base::UTF8ToUTF16(extension->name());

  if (app_list::IsInternalApp(app_id))
    return app_list::GetInternalAppNameById(app_id);

  return base::string16();
}

std::string LauncherControllerHelper::GetAppID(content::WebContents* tab) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    const std::vector<Profile*> profile_list =
        profile_manager->GetLoadedProfiles();
    if (!profile_list.empty()) {
      for (auto* i : profile_list) {
        const extensions::Extension* extension = GetExtensionForTab(i, tab);
        if (extension)
          return extension->id();
      }
      return std::string();
    }
  }
  // If there is no profile manager we only use the known profile.
  const extensions::Extension* extension = GetExtensionForTab(profile_, tab);
  return extension ? extension->id() : std::string();
}

bool LauncherControllerHelper::IsValidIDForCurrentUser(
    const std::string& id) const {
  const ArcAppListPrefs* arc_prefs = GetArcAppListPrefs();
  if (arc_prefs && arc_prefs->IsRegistered(id))
    return true;

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile_);
  if (registry_service && registry_service->IsCrostiniShelfAppId(id)) {
    return crostini::IsCrostiniUIAllowedForProfile(profile_) &&
           registry_service->GetRegistration(id).has_value();
  }

  if (app_list::IsInternalApp(id))
    return true;

  if (!GetExtensionByID(profile_, id))
    return false;
  if (id == arc::kPlayStoreAppId) {
    if (!arc::IsArcAllowedForProfile(profile()) || !arc::IsPlayStoreAvailable())
      return false;
    const arc::ArcSessionManager* arc_session_manager =
        arc::ArcSessionManager::Get();
    DCHECK(arc_session_manager);
    if (!arc_session_manager->IsAllowed())
      return false;
    if (!arc::IsArcPlayStoreEnabledForProfile(profile()) &&
        arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()))
      return false;
  }

  return true;
}

void LauncherControllerHelper::LaunchApp(const ash::ShelfID& id,
                                         ash::ShelfLaunchSource source,
                                         int event_flags,
                                         int64_t display_id) {
  const std::string& app_id = id.app_id;
  const ArcAppListPrefs* arc_prefs = GetArcAppListPrefs();
  if (arc_prefs && arc_prefs->IsRegistered(app_id)) {
    arc::LaunchApp(profile_, app_id, event_flags,
                   arc::UserInteractionType::APP_STARTED_FROM_SHELF,
                   display_id);
    return;
  }

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile_);
  if (registry_service && registry_service->IsCrostiniShelfAppId(app_id)) {
    // This expects a valid app list id, which is fine as we only get here for
    // shelf entries associated with an actual app and not arbitrary Crostini
    // windows.
    crostini::LaunchCrostiniApp(profile_, app_id, display_id);
    return;
  }

  if (app_list::IsInternalApp(app_id)) {
    app_list::OpenInternalApp(app_id, profile_, event_flags);
    return;
  }

  // |extension| could be null when it is being unloaded for updating.
  const extensions::Extension* extension = GetExtensionByID(profile_, app_id);
  if (!extension)
    return;

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    // Do nothing if there is already a running enable flow.
    if (extension_enable_flow_)
      return;

    extension_enable_flow_.reset(
        new ExtensionEnableFlow(profile_, app_id, this));
    extension_enable_flow_->StartForNativeWindow(nullptr);
    return;
  }

  // The app will be created for the currently active profile.
  AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
      profile_, extension, event_flags, extensions::SOURCE_APP_LAUNCHER,
      display_id);
  if (source != ash::LAUNCH_FROM_UNKNOWN &&
      app_id == extensions::kWebStoreAppId) {
    // Get the corresponding source string.
    std::string source_value = GetSourceFromAppListSource(source);

    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url, extension_urls::kWebstoreSourceField, source_value);
  }
  params.launch_id = id.launch_id;

  OpenApplication(params);
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
