// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/system_web_app_manager.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // OS_CHROMEOS

namespace web_app {

namespace {

PendingAppManager::AppInfo CreateAppInfoForSystemApp(const GURL& url) {
  DCHECK_EQ(content::kChromeUIScheme, url.scheme());
  return {
      url,
      LaunchContainer::kWindow,
      InstallSource::kSystemInstalled,
      false /* create_shortcuts */,
      PendingAppManager::AppInfo::kDefaultOverridePreviousUserUninstall,
      true /* bypass_service_worker_check */,
  };
}

}  // namespace

SystemWebAppManager::SystemWebAppManager(Profile* profile,
                                         PendingAppManager* pending_app_manager)
    : profile_(profile), pending_app_manager_(pending_app_manager) {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      base::BindOnce(&SystemWebAppManager::StartAppInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

SystemWebAppManager::~SystemWebAppManager() = default;

// static
bool SystemWebAppManager::ShouldEnableForProfile(Profile* profile) {
  bool is_enabled = base::FeatureList::IsEnabled(features::kSystemWebApps);
#if defined(OS_CHROMEOS)
  // System Apps should not be installed to the signin profile.
  is_enabled = is_enabled && !chromeos::ProfileHelper::IsSigninProfile(profile);
#endif
  return is_enabled;
}

void SystemWebAppManager::StartAppInstallation() {
  std::vector<GURL> urls_to_install;
  if (ShouldEnableForProfile(profile_)) {
    // Skipping this will uninstall all System Apps currently installed.
    urls_to_install = CreateSystemWebApps();
  }

  std::vector<PendingAppManager::AppInfo> apps_to_install;
  for (const auto& url : urls_to_install)
    apps_to_install.emplace_back(CreateAppInfoForSystemApp(url));

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(apps_to_install), InstallSource::kSystemInstalled);
}

std::vector<GURL> SystemWebAppManager::CreateSystemWebApps() {
  std::vector<GURL> urls;

// TODO(calamity): Split this into per-platform functions.
#if defined(OS_CHROMEOS)
  urls.emplace_back(chrome::kChromeUIDiscoverURL);
  constexpr char kChromeSettingsPWAURL[] = "chrome://settings/pwa.html";
  urls.emplace_back(kChromeSettingsPWAURL);
#endif  // OS_CHROMEOS

  return urls;
}

}  // namespace web_app
