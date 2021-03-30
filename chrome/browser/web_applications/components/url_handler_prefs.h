// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_PREFS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_PREFS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/components/url_handler_launch_params.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "url/gurl.h"

class PrefRegistrySimple;

namespace base {
class FilePath;
}  // namespace base

namespace web_app {

// Manage web app URL handler information in local state prefs.
// These prefs aggregate information from web apps installed to all user
// profiles.
//
// TODO(crbug/1072058): Deduplicate this with the the App Service's intent
// handling system. For eg., this could be replaced with a cache of App Service
// information implemented with local state prefs and managed by App Service
// itself.
//
// The information stored here can be accessed when user profiles and
// |WebAppRegistrar|s are not yet loaded, for example during browser startup.
// At the topmost level, prefs are mapped by origin. If an app manifest
// contained a "url_handlers" field with an "*.contoso.com" origin, an entry
// will be added here under the "https://contoso.com" key. The mapped value
// contains a list of handlers, each of which identifies the app_id and profile
// of the app that could be launched. It also contains "paths" and
// "exclude_paths" patterns for more specific matches. it also contains
// information about user permissions and saved defaults.
//
// An example of the information stored using this model:
// {
//     "https://contoso.com":
//     [
//         {
//             "app_id": "dslkfjweiourasdalfjkdslkfjowiesdfwee",
//             "profile_path": "C:\\Users\\alias\\Profile\\Default",
//             "origin": "https://contoso.com",
//             "has_origin_wildcard": false,
//             "paths": ["/*"],
//             "exclude_paths": ["/abc"],
//             "user_permission": true
//         },
//         {
//             "app_id": "qruhrugqrgjdsdfhjghjrghjhdfgaaamenww",
//             "profile_path": "C:\\Users\\alias\\Profile\\Default",
//             "origin": "https://contoso.com",
//             "has_origin_wildcard": true,
//             "paths": [],
//             "exclude_paths": [],
//             "user_permission": false
//         }
//     ],
//     "https://www.en.osotnoc.org": [...]
// }
namespace url_handler_prefs {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

void AddWebApp(PrefService* local_state,
               const AppId& app_id,
               const base::FilePath& profile_path,
               const apps::UrlHandlers& url_handlers);

void UpdateWebApp(PrefService* local_state,
                  const AppId& app_id,
                  const base::FilePath& profile_path,
                  const apps::UrlHandlers& url_handlers);

void RemoveWebApp(PrefService* local_state,
                  const AppId& app_id,
                  const base::FilePath& profile_path);

void RemoveProfile(PrefService* local_state,
                   const base::FilePath& profile_path);

void Clear(PrefService* local_state);

// Search for all (app, profile) combinations that have active URL handlers
// that matches |url|.
// |url| is a fully specified URL, eg. "https://contoso.com/abc/def".
// TODO(crbug/1072058): Filter out inactive handlers when user permission is
// implemented.
std::vector<UrlHandlerLaunchParams> FindMatchingUrlHandlers(
    PrefService* local_state,
    const GURL& url);

}  // namespace url_handler_prefs

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_PREFS_H_
