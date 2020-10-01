// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_app_registrar.h"

#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "url/gurl.h"

namespace web_app {

TestAppRegistrar::TestAppRegistrar() : AppRegistrar(nullptr) {}

TestAppRegistrar::~TestAppRegistrar() = default;

void TestAppRegistrar::AddExternalApp(const AppId& app_id,
                                      const AppInfo& info) {
  installed_apps_[app_id] = info;
}

void TestAppRegistrar::RemoveExternalApp(const AppId& app_id) {
  DCHECK(base::Contains(installed_apps_, app_id));
  installed_apps_.erase(app_id);
}

void TestAppRegistrar::RemoveExternalAppByInstallUrl(const GURL& install_url) {
  RemoveExternalApp(*LookupExternalAppId(install_url));
}

bool TestAppRegistrar::IsInstalled(const AppId& app_id) const {
  return base::Contains(installed_apps_, app_id);
}

bool TestAppRegistrar::IsLocallyInstalled(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return false;
}

bool TestAppRegistrar::WasInstalledByUser(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return false;
}

std::map<AppId, GURL> TestAppRegistrar::GetExternallyInstalledApps(
    ExternalInstallSource install_source) const {
  std::map<AppId, GURL> apps;
  for (auto& id_and_info : installed_apps_) {
    if (id_and_info.second.source == install_source)
      apps[id_and_info.first] = id_and_info.second.install_url;
  }

  return apps;
}

base::Optional<AppId> TestAppRegistrar::LookupExternalAppId(
    const GURL& install_url) const {
  auto it = std::find_if(installed_apps_.begin(), installed_apps_.end(),
                         [install_url](const auto& app_it) {
                           return app_it.second.install_url == install_url;
                         });
  return it == installed_apps_.end() ? base::Optional<AppId>() : it->first;
}

bool TestAppRegistrar::HasExternalAppWithInstallSource(
    const AppId& app_id,
    ExternalInstallSource install_source) const {
  auto it = std::find_if(installed_apps_.begin(), installed_apps_.end(),
                         [app_id, install_source](const auto& app_it) {
                           return app_it.first == app_id &&
                                  app_it.second.source == install_source;
                         });
  return it != installed_apps_.end();
}

int TestAppRegistrar::CountUserInstalledApps() const {
  NOTIMPLEMENTED();
  return 0;
}

std::string TestAppRegistrar::GetAppShortName(const AppId&) const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string TestAppRegistrar::GetAppDescription(const AppId&) const {
  NOTIMPLEMENTED();
  return std::string();
}

base::Optional<SkColor> TestAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<SkColor> TestAppRegistrar::GetAppBackgroundColor(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

const GURL& TestAppRegistrar::GetAppStartUrl(const AppId& app_id) const {
  auto iterator = installed_apps_.find(app_id);
  if (iterator == installed_apps_.end())
    return GURL::EmptyGURL();

  return iterator->second.launch_url;
}

const std::string* TestAppRegistrar::GetAppLaunchQueryParams(
    const AppId& app_id) const {
  return nullptr;
}

const apps::ShareTarget* TestAppRegistrar::GetAppShareTarget(
    const AppId& app_id) const {
  return nullptr;
}

base::Optional<GURL> TestAppRegistrar::GetAppScopeInternal(
    const AppId& app_id) const {
  const auto& result = installed_apps_.find(app_id);
  if (result == installed_apps_.end())
    return base::nullopt;

  return base::make_optional(result->second.install_url);
}

DisplayMode TestAppRegistrar::GetAppDisplayMode(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return DisplayMode::kBrowser;
}

DisplayMode TestAppRegistrar::GetAppUserDisplayMode(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return DisplayMode::kBrowser;
}

std::vector<DisplayMode> TestAppRegistrar::GetAppDisplayModeOverride(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return std::vector<DisplayMode>();
}

base::Time TestAppRegistrar::GetAppLastLaunchTime(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::Time();
}

base::Time TestAppRegistrar::GetAppInstallTime(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::Time();
}

std::vector<WebApplicationIconInfo> TestAppRegistrar::GetAppIconInfos(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return {};
}

SortedSizesPx TestAppRegistrar::GetAppDownloadedIconSizesAny(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return {};
}

std::vector<WebApplicationShortcutsMenuItemInfo>
TestAppRegistrar::GetAppShortcutsMenuItemInfos(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return {};
}

std::vector<std::vector<SquareSizePx>>
TestAppRegistrar::GetAppDownloadedShortcutsMenuIconsSizes(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return {{}};
}

RunOnOsLoginMode TestAppRegistrar::GetAppRunOnOsLoginMode(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return RunOnOsLoginMode::kUndefined;
}

std::vector<AppId> TestAppRegistrar::GetAppIds() const {
  std::vector<AppId> result;
  for (const std::pair<const AppId, AppInfo>& it : installed_apps_) {
    result.push_back(it.first);
  }
  return result;
}

WebAppRegistrar* TestAppRegistrar::AsWebAppRegistrar() {
  return nullptr;
}

}  // namespace web_app
