// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/stl_util.h"

namespace web_app {

const bool PendingAppManager::AppInfo::kDefaultCreateShortcuts = true;
const bool PendingAppManager::AppInfo::kDefaultOverridePreviousUserUninstall =
    false;
const bool PendingAppManager::AppInfo::kDefaultBypassServiceWorkerCheck = false;
const bool PendingAppManager::AppInfo::kDefaultRequireManifest = false;

PendingAppManager::AppInfo::AppInfo(GURL url,
                                    LaunchContainer launch_container,
                                    InstallSource install_source,
                                    bool create_shortcuts,
                                    bool override_previous_user_uninstall,
                                    bool bypass_service_worker_check,
                                    bool require_manifest)
    : url(std::move(url)),
      launch_container(launch_container),
      install_source(install_source),
      create_shortcuts(create_shortcuts),
      override_previous_user_uninstall(override_previous_user_uninstall),
      bypass_service_worker_check(bypass_service_worker_check),
      require_manifest(require_manifest) {}

PendingAppManager::AppInfo::AppInfo(PendingAppManager::AppInfo&& other) =
    default;

PendingAppManager::AppInfo::~AppInfo() = default;

std::unique_ptr<PendingAppManager::AppInfo> PendingAppManager::AppInfo::Clone()
    const {
  std::unique_ptr<AppInfo> other(new AppInfo(
      url, launch_container, install_source, create_shortcuts,
      override_previous_user_uninstall, bypass_service_worker_check));
  DCHECK_EQ(*this, *other);
  return other;
}

bool PendingAppManager::AppInfo::operator==(
    const PendingAppManager::AppInfo& other) const {
  return std::tie(url, launch_container, install_source, create_shortcuts,
                  override_previous_user_uninstall, bypass_service_worker_check,
                  require_manifest) ==
         std::tie(other.url, other.launch_container, other.install_source,
                  other.create_shortcuts,
                  other.override_previous_user_uninstall,
                  other.bypass_service_worker_check, other.require_manifest);
}

PendingAppManager::PendingAppManager() = default;

PendingAppManager::~PendingAppManager() = default;

void PendingAppManager::SynchronizeInstalledApps(
    std::vector<AppInfo> desired_apps,
    InstallSource install_source) {
  DCHECK(std::all_of(desired_apps.begin(), desired_apps.end(),
                     [&install_source](const AppInfo& app_info) {
                       return app_info.install_source == install_source;
                     }));

  std::vector<GURL> current_urls = GetInstalledAppUrls(install_source);
  std::sort(current_urls.begin(), current_urls.end());

  std::vector<GURL> desired_urls;
  for (const auto& info : desired_apps) {
    desired_urls.emplace_back(info.url);
  }
  std::sort(desired_urls.begin(), desired_urls.end());

  UninstallApps(
      base::STLSetDifference<std::vector<GURL>>(current_urls, desired_urls),
      base::DoNothing());
  InstallApps(std::move(desired_apps), base::DoNothing());
}

std::ostream& operator<<(std::ostream& out,
                         const PendingAppManager::AppInfo& app_info) {
  return out << "url: " << app_info.url << "\n launch_container: "
             << static_cast<int32_t>(app_info.launch_container)
             << "\n install_source: "
             << static_cast<int32_t>(app_info.install_source)
             << "\n create_shortcuts: " << app_info.create_shortcuts
             << "\n override_previous_user_uninstall: "
             << app_info.override_previous_user_uninstall
             << "\n bypass_service_worker_check: "
             << app_info.bypass_service_worker_check
             << "\n require_manifest: " << app_info.require_manifest;
}

}  // namespace web_app
