// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// TODO(http://b/333583704): Revert CL which added this struct after migration.
struct DeviceInfo {
  DeviceInfo();
  DeviceInfo(const DeviceInfo&);
  DeviceInfo(DeviceInfo&&);
  DeviceInfo& operator=(const DeviceInfo&);
  DeviceInfo& operator=(DeviceInfo&&);
  ~DeviceInfo();

#if BUILDFLAG(IS_CHROMEOS)
  // The OOBE timestamp corresponding to the time of device registration.
  // If absent, the timestamp is unavailable. This is known to occur during
  // first boot due to a race condition between device registration and
  // preinstallation.
  std::optional<base::Time> oobe_timestamp;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// An allow list for a set of preinstalled web app that will be installed even
// if `switches::kDisableDefaultApps` is set.
using PreinstallUrlAllowList = std::optional<base::flat_set<GURL>>;
PreinstallUrlAllowList& GetPreinstallUrlAllowListForTesting();

// Temporarily sets an allow list for a set of install URLs that will be
// permitted to be preinstalled even if `switches::kDisableDefaultApps` is set.
// This allows tests to enable particular preinstalled web apps without enabling
// all of them. Enabling all has the side effect of installing a queue of web
// apps, some of which require network access that hit a 30s timeout period (see
// `WebAppUrlLoader`), causing the test to timeout when waiting on a particular
// web app that happens to get placed at the end of the install queue.
// By only enabling the necessary ones this avoids test timeouts caused by
// unrelated web app preinstalls.
using ScopedPreinstallUrlAllowList =
    base::AutoReset<std::optional<base::flat_set<GURL>>>;
ScopedPreinstallUrlAllowList SetPreinstallUrlAllowListForTesting(
    PreinstallUrlAllowList preinstall_url_allow_list);

bool PreinstalledWebAppsDisabled();

// Returns the list of web apps that should be pre-installed on new profiles.
std::vector<ExternalInstallOptions> GetPreinstalledWebApps(
    Profile& profile,
    const std::optional<DeviceInfo>& device_info = std::nullopt);

// A subset of ExternalInstallOptions pertaining to web app migration.
struct PreinstalledWebAppMigration {
  PreinstalledWebAppMigration();
  PreinstalledWebAppMigration(PreinstalledWebAppMigration&&) noexcept;
  ~PreinstalledWebAppMigration();

  GURL install_url;
  webapps::AppId expected_web_app_id;
  webapps::AppId old_chrome_app_id;
};

// Returns the list of preinstalled web apps that are migrations away from their
// corresponding Chrome app.
std::vector<PreinstalledWebAppMigration> GetPreinstalledWebAppMigrations(
    Profile& profile);

// A scoped helper to provide a testing set of preinstalled app data. This will
// replace the default set.
struct ScopedTestingPreinstalledAppData {
  ScopedTestingPreinstalledAppData();
  ScopedTestingPreinstalledAppData(const ScopedTestingPreinstalledAppData&) =
      delete;
  ScopedTestingPreinstalledAppData& operator=(
      const ScopedTestingPreinstalledAppData&) = delete;
  ~ScopedTestingPreinstalledAppData();

  std::vector<ExternalInstallOptions> apps;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_
