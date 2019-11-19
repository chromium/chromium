// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNALLY_INSTALLED_WEB_APP_PREFS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNALLY_INSTALLED_WEB_APP_PREFS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class GURL;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// A Prefs-backed map from web app URLs to app IDs and their InstallSources.
//
// This lets us determine, given a web app's URL, whether that web app was
// already installed by a non-user external source e.g. policy or Chrome default
// and system apps.
class ExternallyInstalledWebAppPrefs {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static bool HasAppId(const PrefService* pref_service, const AppId& app_id);

  // Returns true if |app_id| was added with |install_source| to
  // |pref_service|.
  static bool HasAppIdWithInstallSource(const PrefService* pref_service,
                                        const AppId& app_id,
                                        ExternalInstallSource install_source);

  // Returns the URLs of the apps that have been installed from
  // |install_source|. Will still return apps that have been uninstalled.
  static std::map<AppId, GURL> BuildAppIdsMap(
      const PrefService* pref_service,
      ExternalInstallSource install_source);

  explicit ExternallyInstalledWebAppPrefs(PrefService* pref_service);

  void Insert(const GURL& url,
              const AppId& app_id,
              ExternalInstallSource install_source);
  base::Optional<AppId> LookupAppId(const GURL& url) const;

  // Returns an id if there is a placeholder app for |url|. Note that nullopt
  // does not mean that there is no app for |url| just that there is no
  // *placeholder app*.
  base::Optional<AppId> LookupPlaceholderAppId(const GURL& url) const;
  void SetIsPlaceholder(const GURL& url, bool is_placeholder);
  bool IsPlaceholderApp(const AppId& app_id) const;

 private:
  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ExternallyInstalledWebAppPrefs);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNALLY_INSTALLED_WEB_APP_PREFS_H_
