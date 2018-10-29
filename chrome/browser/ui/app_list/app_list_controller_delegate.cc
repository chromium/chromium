// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/extension_uninstaller.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "net/base/url_util.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

using extensions::ExtensionRegistry;

namespace {

const extensions::Extension* GetExtension(Profile* profile,
                                          const std::string& extension_id) {
  const ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(extension_id);
  return extension;
}

}  // namespace

AppListControllerDelegate::AppListControllerDelegate()
    : is_home_launcher_enabled_(app_list_features::IsHomeLauncherEnabled()),
      weak_ptr_factory_(this) {}

AppListControllerDelegate::~AppListControllerDelegate() {}

void AppListControllerDelegate::GetAppInfoDialogBounds(
    GetAppInfoDialogBoundsCallback callback) {
  std::move(callback).Run(gfx::Rect());
}

std::string AppListControllerDelegate::AppListSourceToString(
    AppListSource source) {
  switch (source) {
    case LAUNCH_FROM_APP_LIST:
      return extension_urls::kLaunchSourceAppList;
    case LAUNCH_FROM_APP_LIST_SEARCH:
      return extension_urls::kLaunchSourceAppListSearch;
    default:
      return std::string();
  }
}

bool AppListControllerDelegate::UserMayModifySettings(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile)->management_policy();
  return extension &&
         policy->UserMayModifySettings(extension, NULL);
}

bool AppListControllerDelegate::CanDoShowAppInfoFlow() {
  return CanShowAppInfoDialog();
}

void AppListControllerDelegate::DoShowAppInfoFlow(
    Profile* profile,
    const std::string& extension_id) {
  DCHECK(CanDoShowAppInfoFlow());

  const extensions::Extension* extension = GetExtension(profile, extension_id);
  DCHECK(extension);
  if (extension->is_hosted_app() && extension->from_bookmark()) {
    chrome::ShowSiteSettings(
        profile, extensions::AppLaunchInfo::GetFullLaunchURL(extension));
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Apps.AppInfoDialog.Launches",
                            AppInfoLaunchSource::FROM_APP_LIST,
                            AppInfoLaunchSource::NUM_LAUNCH_SOURCES);

  // Since the AppListControllerDelegate is a leaky singleton, passing its raw
  // pointer around is OK.
  GetAppInfoDialogBounds(base::BindOnce(
      [](base::WeakPtr<AppListControllerDelegate> self, Profile* profile,
         const std::string& extension_id, const gfx::Rect& bounds) {
        const extensions::Extension* extension =
            GetExtension(profile, extension_id);
        DCHECK(extension);
        ShowAppInfoInAppList(bounds, profile, extension);
      },
      weak_ptr_factory_.GetWeakPtr(), profile, extension_id));
}

void AppListControllerDelegate::UninstallApp(Profile* profile,
                                             const std::string& app_id) {
  // ExtensionUninstall deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller = new ExtensionUninstaller(profile, app_id);
  uninstaller->Run();
}

bool AppListControllerDelegate::IsAppFromWebStore(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  return extension && extension->from_webstore();
}

void AppListControllerDelegate::ShowAppInWebStore(
    Profile* profile,
    const std::string& app_id,
    bool is_search_result) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  if (!extension)
    return;

  const GURL url = extensions::ManifestURL::GetDetailsURL(extension);
  DCHECK_NE(url, GURL::EmptyGURL());

  const std::string source = AppListSourceToString(
      is_search_result ?
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST_SEARCH :
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST);
  OpenURL(profile, net::AppendQueryParameter(
                       url, extension_urls::kWebstoreSourceField, source),
          ui::PAGE_TRANSITION_LINK, WindowOpenDisposition::CURRENT_TAB);
}

bool AppListControllerDelegate::HasOptionsPage(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  return extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile) &&
         extension && extensions::OptionsPageInfo::HasOptionsPage(extension);
}

void AppListControllerDelegate::ShowOptionsPage(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  if (!extension)
    return;

  OpenURL(profile, extensions::OptionsPageInfo::GetOptionsPage(extension),
          ui::PAGE_TRANSITION_LINK, WindowOpenDisposition::CURRENT_TAB);
}

extensions::LaunchType AppListControllerDelegate::GetExtensionLaunchType(
    Profile* profile,
    const std::string& app_id) {
  return extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile),
                                   GetExtension(profile, app_id));
}

void AppListControllerDelegate::SetExtensionLaunchType(
    Profile* profile,
    const std::string& extension_id,
    extensions::LaunchType launch_type) {
  extensions::SetLaunchType(profile, extension_id, launch_type);
}

bool AppListControllerDelegate::IsExtensionInstalled(
    Profile* profile, const std::string& app_id) {
  return !!GetExtension(profile, app_id);
}

extensions::InstallTracker* AppListControllerDelegate::GetInstallTrackerFor(
    Profile* profile) {
  if (extensions::ExtensionSystem::Get(profile)->extension_service())
    return extensions::InstallTrackerFactory::GetForBrowserContext(profile);
  return NULL;
}

void AppListControllerDelegate::GetApps(Profile* profile,
                                        extensions::ExtensionSet* out_apps) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  DCHECK(registry);
  out_apps->InsertAll(registry->enabled_extensions());
  out_apps->InsertAll(registry->disabled_extensions());
  out_apps->InsertAll(registry->terminated_extensions());
}

void AppListControllerDelegate::OnSearchStarted() {
#if BUILDFLAG(ENABLE_RLZ)
  rlz::RLZTracker::RecordAppListSearch();
#endif
}

bool AppListControllerDelegate::IsHomeLauncherEnabledInTabletMode() const {
  return is_home_launcher_enabled_ && TabletModeClient::Get() &&
         TabletModeClient::Get()->tablet_mode_enabled();
}
