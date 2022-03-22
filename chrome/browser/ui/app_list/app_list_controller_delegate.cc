// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/tablet_mode_page_behavior.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/settings/ash/app_management/app_management_uma.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
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

AppListControllerDelegate::AppListControllerDelegate() {}

AppListControllerDelegate::~AppListControllerDelegate() {}

void AppListControllerDelegate::DoShowAppInfoFlow(Profile* profile,
                                                  const std::string& app_id) {
  auto app_type = apps::AppServiceProxyFactory::GetForProfile(profile)
                      ->AppRegistryCache()
                      .GetAppType(app_id);
  DCHECK_NE(app_type, apps::AppType::kUnknown);

  if (app_type == apps::AppType::kWeb ||
      app_type == apps::AppType::kSystemWeb) {
    chrome::ShowAppManagementPage(profile, app_id,
                                  ash::settings::AppManagementEntryPoint::
                                      kAppListContextMenuAppInfoWebApp);
  } else {
    chrome::ShowAppManagementPage(profile, app_id,
                                  ash::settings::AppManagementEntryPoint::
                                      kAppListContextMenuAppInfoChromeApp);
  }
}

void AppListControllerDelegate::UninstallApp(Profile* profile,
                                             const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile)->Uninstall(
      app_id, apps::mojom::UninstallSource::kAppList, GetAppListWindow());
}

void AppListControllerDelegate::ShowOptionsPage(Profile* profile,
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

void AppListControllerDelegate::OnSearchStarted() {
#if BUILDFLAG(ENABLE_RLZ)
  rlz::RLZTracker::RecordAppListSearch();
#endif
}
