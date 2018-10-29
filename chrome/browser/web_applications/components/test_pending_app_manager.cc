// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/test_pending_app_manager.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "url/gurl.h"

namespace web_app {

TestPendingAppManager::TestPendingAppManager()
    : deduped_install_count_(0), deduped_uninstall_count_(0) {}

TestPendingAppManager::~TestPendingAppManager() = default;

void TestPendingAppManager::SimulatePreviouslyInstalledApp(
    const GURL& url,
    InstallSource install_source) {
  installed_apps_[url] = install_source;
}

void TestPendingAppManager::Install(AppInfo app_to_install,
                                    OnceInstallCallback callback) {
  // TODO(nigeltao): Add error simulation when error codes are added to the API.

  auto i = installed_apps_.find(app_to_install.url);
  if (i == installed_apps_.end()) {
    installed_apps_[app_to_install.url] = app_to_install.install_source;
    deduped_install_count_++;
  }

  install_requests_.push_back(std::move(app_to_install));
  std::move(callback).Run(install_requests().back().url,
                          InstallResultCode::kSuccess);
}

void TestPendingAppManager::InstallApps(
    std::vector<AppInfo> apps_to_install,
    const RepeatingInstallCallback& callback) {
  for (auto& app : apps_to_install)
    Install(std::move(app), callback);
}

void TestPendingAppManager::UninstallApps(std::vector<GURL> urls_to_uninstall,
                                          const UninstallCallback& callback) {
  for (auto& url : urls_to_uninstall) {
    auto i = installed_apps_.find(url);
    if (i != installed_apps_.end()) {
      installed_apps_.erase(i);
      deduped_uninstall_count_++;
    }

    uninstall_requests_.push_back(url);
    callback.Run(url, true /* succeeded */);
  }
}

std::vector<GURL> TestPendingAppManager::GetInstalledAppUrls(
    InstallSource install_source) const {
  std::vector<GURL> urls;
  for (auto& it : installed_apps_) {
    if (it.second == install_source) {
      urls.emplace_back(it.first);
    }
  }
  return urls;
}

}  // namespace web_app
