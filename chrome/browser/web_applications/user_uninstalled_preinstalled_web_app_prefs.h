// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_USER_UNINSTALLED_PREINSTALLED_WEB_APP_PREFS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_USER_UNINSTALLED_PREINSTALLED_WEB_APP_PREFS_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class GURL;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {
// A Prefs-backed map from preinstalled web app IDs to their install URLs.
//
// Preinstalled apps are the only type of externally installed apps that
// can be uninstalled by user. So if an user has uninstalled a preinstalled app,
// then it should stay uninstalled on startup.
//
// To prevent that, we keep track of the install URLs of preinstalled apps
// outside of the web_app DB so that on every startup, the WebAppSystem can
// keep the user uninstalled preinstalled apps uninstalled,
// thereby maintaining its synchronization.
//
// The prefs are stored in prefs::kUserUninstalledPreinstalledWebAppPref and
// they are stored as map<webapps::AppId, Set<Install URLs>>, e.g.
// {"app_id": {"https://install_url1.com", "https://install_url2.com"}}
//
// They can be seen on chrome://web-app-internals under the
// PreinstalledAppsUninstalledByUserConfigs json for debugging purposes.
class UserUninstalledPreinstalledWebAppPrefs {
 public:
  static const char kUserUninstalledPreinstalledAppAction[];
  explicit UserUninstalledPreinstalledWebAppPrefs(PrefService* pref_service);
  UserUninstalledPreinstalledWebAppPrefs(
      const UserUninstalledPreinstalledWebAppPrefs&) = delete;
  UserUninstalledPreinstalledWebAppPrefs& operator=(
      const UserUninstalledPreinstalledWebAppPrefs&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  void Add(const webapps::AppId& app_id, base::flat_set<GURL> install_urls);
  std::optional<webapps::AppId> LookUpAppIdByInstallUrl(
      const GURL& install_url);
  bool DoesAppIdExist(const webapps::AppId& app_id);
  void AppendExistingInstallUrlsPerAppId(const webapps::AppId& app_id,
                                         base::flat_set<GURL>& urls);
  int Size();
  bool RemoveByInstallUrl(const webapps::AppId& app_id,
                          const GURL& install_url);
  bool RemoveByAppId(const webapps::AppId& app_id);
  bool AppIdContainsAllUrls(
      const webapps::AppId& app_id,
      const base::flat_map<WebAppManagement::Type,
                           WebApp::ExternalManagementConfig>& url_map,
      const bool only_default);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Clear all apps marked as user uninstalled. Only used for Lacros
  // disablement.
  void ClearAllApps();
#endif

 private:
  const raw_ptr<PrefService> pref_service_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_USER_UNINSTALLED_PREINSTALLED_WEB_APP_PREFS_H_
