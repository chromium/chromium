// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_URL_HANDLER_PREFS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_URL_HANDLER_PREFS_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/url_handler_launch_params.h"
#include "chrome/browser/web_applications/web_app_id.h"
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
// of the app that could be launched. It also contains "include_paths" and
// "exclude_paths" patterns for more specific matches. It also contains saved
// user preferences.
//
// An example of the information stored using this model:
// {
//     "https://contoso.com":
//     [
//         {
//             "app_id": "dslkfjweiourasdalfjkdslkfjowiesdfwee",
//             "profile_path": "C:\\Users\\alias\\Profile\\Default",
//             "has_origin_wildcard": false,
//             "include_paths": [
//                 {
//                   "path": "/*",
//                   "choice": 2,  // kInApp
//                   // "2000-01-01 00:00:00.000 UTC"
//                   "timestamp": "12591158400000000"
//                 }
//             ],
//             "exclude_paths": ["/abc"],
//         },
//         {
//             "app_id": "qruhrugqrgjdsdfhjghjrghjhdfgaaamenww",
//             "profile_path": "C:\\Users\\alias\\Profile\\Default",
//             "has_origin_wildcard": true,
//             "include_paths": [],
//             "exclude_paths": [],
//         }
//     ],
//     "https://www.en.osotnoc.org": [...]
// }
namespace url_handler_prefs {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

void AddWebApp(PrefService* local_state,
               const AppId& app_id,
               const base::FilePath& profile_path,
               const apps::UrlHandlers& url_handlers,
               const base::Time& time = base::Time::Now());

void UpdateWebApp(PrefService* local_state,
                  const AppId& app_id,
                  const base::FilePath& profile_path,
                  apps::UrlHandlers new_url_handlers,
                  const base::Time& time = base::Time::Now());

void RemoveWebApp(PrefService* local_state,
                  const AppId& app_id,
                  const base::FilePath& profile_path);

void RemoveProfile(PrefService* local_state,
                   const base::FilePath& profile_path);

// Returns true if there are any apps with valid 'url_handlers' installed to the
// profile at 'profile_path'.
bool ProfileHasUrlHandlers(PrefService* local_state,
                           const base::FilePath& profile_path);

void Clear(PrefService* local_state);

// Returns true if any path dictionary in |handler| matches |profile_path|.
bool IsHandlerForProfile(const base::Value& handler,
                         const base::FilePath& profile_path);

// Search for all (app, profile) combinations that have active URL handlers
// which matches `url`.
// `url` is a fully specified URL, eg. "https://contoso.com/abc/def".
// If the most recent match is saved as kInApp, only it will be returned;
// If saved as kNone, all the matches regardless of saved_choice value are
// returned; If saved as kInBrowser, the return value is empty as the preferred
// choice is the browser.
std::vector<UrlHandlerLaunchParams> FindMatchingUrlHandlers(
    PrefService* local_state,
    const GURL& url);

// Users can save their app choice from the intent picker dialog so that they
// are not prompted again the next time a similar URL matches to the same app.
void SaveOpenInApp(PrefService* local_state,
                   const AppId& app_id,
                   const base::FilePath& profile_path,
                   const GURL& url,
                   const base::Time& time = base::Time::Now());

// Users can save their choice to not launch a web app when a similar URL is
// matched in the future.
void SaveOpenInBrowser(PrefService* local_state,
                       const GURL& url,
                       const base::Time& time = base::Time::Now());

// Users can reset previously saved choices from the settings page. This
// function can be used by the settings page to reset both |kOpenInApp| and
// |kOpenInBrowser| choices. If app_id.has_value() is false, matching entries
// from all apps will be reset.
void ResetSavedChoice(PrefService* local_state,
                      const absl::optional<std::string>& app_id,
                      const base::FilePath& profile_path,
                      const std::string& origin,
                      bool has_origin_wildcard,
                      const std::string& url_path,
                      const base::Time& time = base::Time::Now());

// Used to hold the pieces of handler information needed for saving default
// choices.
struct HandlerView {
  const raw_ref<const std::string> app_id;
  base::FilePath profile_path;
  bool has_origin_wildcard;
  const raw_ref<base::Value::List> include_paths;
  const raw_ref<base::Value::List> exclude_paths;
};

absl::optional<const HandlerView> GetConstHandlerView(
    const base::Value& handler);

absl::optional<HandlerView> GetHandlerView(base::Value& handler);

}  // namespace url_handler_prefs

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_URL_HANDLER_PREFS_H_
