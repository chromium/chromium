// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRAR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/web_application_info.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace web_app {

// Deprecated. Please use TestWebAppRegistryController instead.
class TestAppRegistrar : public AppRegistrar {
 public:
  struct AppInfo {
    GURL install_url;
    ExternalInstallSource source = ExternalInstallSource::kExternalDefault;
    GURL launch_url;
  };

  TestAppRegistrar();
  ~TestAppRegistrar() override;

  // Adds |url| to the map of installed apps and returns the generated AppId.
  void AddExternalApp(const AppId& app_id, const AppInfo& info);

  // Removes an app from the map of installed apps.
  void RemoveExternalApp(const AppId& app_id);
  void RemoveExternalAppByInstallUrl(const GURL& install_url);

  // AppRegistrar
  bool IsInstalled(const AppId& app_id) const override;
  bool IsLocallyInstalled(const AppId& app_id) const override;
  bool WasInstalledByUser(const AppId& app_id) const override;
  std::map<AppId, GURL> GetExternallyInstalledApps(
      ExternalInstallSource install_source) const override;
  base::Optional<AppId> LookupExternalAppId(
      const GURL& install_url) const override;
  bool HasExternalAppWithInstallSource(
      const AppId& app_id,
      ExternalInstallSource install_source) const override;
  int CountUserInstalledApps() const override;
  std::string GetAppShortName(const AppId& app_id) const override;
  std::string GetAppDescription(const AppId& app_id) const override;
  base::Optional<SkColor> GetAppThemeColor(const AppId& app_id) const override;
  base::Optional<SkColor> GetAppBackgroundColor(
      const AppId& app_id) const override;
  const GURL& GetAppStartUrl(const AppId& app_id) const override;
  const std::string* GetAppLaunchQueryParams(
      const AppId& app_id) const override;
  const apps::ShareTarget* GetAppShareTarget(
      const AppId& app_id) const override;
  base::Optional<GURL> GetAppScopeInternal(const AppId& app_id) const override;
  DisplayMode GetAppDisplayMode(const AppId& app_id) const override;
  DisplayMode GetAppUserDisplayMode(const AppId& app_id) const override;
  std::vector<DisplayMode> GetAppDisplayModeOverride(
      const AppId& app_id) const override;
  base::Time GetAppLastLaunchTime(const web_app::AppId& app_id) const override;
  base::Time GetAppInstallTime(const web_app::AppId& app_id) const override;
  std::vector<WebApplicationIconInfo> GetAppIconInfos(
      const AppId& app_id) const override;
  SortedSizesPx GetAppDownloadedIconSizesAny(
      const AppId& app_id) const override;
  std::vector<WebApplicationShortcutsMenuItemInfo> GetAppShortcutsMenuItemInfos(
      const AppId& app_id) const override;
  std::vector<std::vector<SquareSizePx>>
  GetAppDownloadedShortcutsMenuIconsSizes(const AppId& app_id) const override;
  RunOnOsLoginMode GetAppRunOnOsLoginMode(const AppId& app_id) const override;
  std::vector<AppId> GetAppIds() const override;
  WebAppRegistrar* AsWebAppRegistrar() override;

 private:
  std::map<AppId, AppInfo> installed_apps_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRAR_H_
