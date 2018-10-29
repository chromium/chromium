// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_IDS_MAP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_IDS_MAP_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"

class GURL;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// A Prefs-backed map from web app URLs to Chrome extension IDs and their
// InstallSources.
//
// This lets us determine, given a web app's URL, whether that web app is
// already installed.
class ExtensionIdsMap {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // TODO(nigeltao): delete this after M72 has branched.
  //
  // This is public only for testing.
  static void UpgradeFromM70Format(PrefService* pref_service);

  static bool HasExtensionId(const PrefService* pref_service,
                             const std::string& extension_id);

  // Returns the URLs of the apps that were installed from |install_source|.
  static std::vector<GURL> GetInstalledAppUrls(Profile* profile,
                                               InstallSource install_source);

  explicit ExtensionIdsMap(PrefService* pref_service);

  void Insert(const GURL& url,
              const std::string& extension_id,
              InstallSource install_source);
  base::Optional<std::string> LookupExtensionId(const GURL& url);

 private:
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIdsMap);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_IDS_MAP_H_
