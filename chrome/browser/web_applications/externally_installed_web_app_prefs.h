// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_INSTALLED_WEB_APP_PREFS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_INSTALLED_WEB_APP_PREFS_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {
// TODO(crbug.com/1339965): Retire ExternallyInstalledWebAppPrefs
// completely if metrics being measured in
// externally_installed_prefs_migration_metrics.h look good.

// This field is being retired from prefs::kWebAppsPreferences, but is
// needed for migration to UserUninstalledPreinstalledWebAppPrefs.
constexpr char kWasExternalAppUninstalledByUser[] =
    "was_external_app_uninstalled_by_user";

// A Prefs-backed map from web app URLs to app IDs and their InstallSources.
//
// This lets us determine, given a web app's URL, whether that web app was
// already installed by a non-user external source e.g. policy or Chrome default
// and system apps.
class ExternallyInstalledWebAppPrefs {
 public:
  // Used in the migration to the web_app DB.
  using ParsedPrefs = base::flat_map<AppId, WebApp::ExternalConfigMap>;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit ExternallyInstalledWebAppPrefs(PrefService* pref_service);
  ExternallyInstalledWebAppPrefs(const ExternallyInstalledWebAppPrefs&) =
      delete;
  ExternallyInstalledWebAppPrefs& operator=(
      const ExternallyInstalledWebAppPrefs&) = delete;

  void Insert(const GURL& url,
              const AppId& app_id,
              ExternalInstallSource install_source);

  void SetIsPlaceholder(const GURL& url, bool is_placeholder);

  // Converts the existing external_pref information to a map<AppId,
  // ParsedPrefs> for simplified parsing and migrating to the web app DB.
  static ParsedPrefs ParseExternalPrefsToWebAppData(PrefService* pref_service);

  // Used to migrate the external pref data to the installed web_app DB.
  static void MigrateExternalPrefData(PrefService* pref_service,
                                      WebAppSyncBridge* sync_bridge);

 private:
  friend class WebAppRegistrar;
  friend class PreinstalledWebAppDuplicationFixer;

  bool Remove(const GURL& url);

  absl::optional<AppId> LookupAppId(const GURL& url) const;

  static bool HasAppId(const PrefService* pref_service, const AppId& app_id);

  // Returns the URLs of the apps that have been installed from
  // |install_source|. Will still return apps that have been uninstalled.
  static base::flat_map<AppId, base::flat_set<GURL>> BuildAppIdsMap(
      const PrefService* pref_service,
      ExternalInstallSource install_source);

  // Returns true if |app_id| was added with |install_source| to
  // |pref_service|.
  static bool HasAppIdWithInstallSource(const PrefService* pref_service,
                                        const AppId& app_id,
                                        ExternalInstallSource install_source);

  // Returns an id if there is a placeholder app for |url|. Note that nullopt
  // does not mean that there is no app for |url| just that there is no
  // *placeholder app*.
  absl::optional<AppId> LookupPlaceholderAppId(const GURL& url) const;

  bool IsPlaceholderApp(const AppId& app_id) const;

  // Used to migrate information regarding user uninstalled preinstalled apps
  // to UserUninstalledPreinstalledWebAppPrefs.
  static void MigrateExternalPrefDataToPreinstalledPrefs(
      PrefService* pref_service,
      const WebAppRegistrar* registrar,
      const ParsedPrefs& parsed_data);

  static base::flat_set<GURL> MergeAllUrls(
      const WebApp::ExternalConfigMap& source_config_map);

  static void LogDataMetrics(bool data_exists_in_pref,
                             bool data_exists_in_registrar);

  const raw_ptr<PrefService, DanglingUntriaged> pref_service_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_INSTALLED_WEB_APP_PREFS_H_
