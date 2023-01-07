// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATION_PREFS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATION_PREFS_UTILS_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace url {
class Origin;
}

namespace web_app {

class WebApp;

extern const char kStorageIsolation[];

void IsolationPrefsUtilsRegisterProfilePrefs(PrefRegistrySimple* registry);

// Updates |web_app|'s entry in the "isolation_state" dictionary to be in sync
// with its current isolation state.
void RecordOrRemoveAppIsolationState(PrefService* pref_service,
                                     const WebApp& web_app);

// Removes |web_app|'s entry in the "isolation_state" dictionary. Must be called
// when uninstalling an app.
void RemoveAppIsolationState(PrefService* pref_service,
                             const url::Origin& origin);

// Returns the storage isolation key to use when loading resources
// from |origin|.
const std::string* GetStorageIsolationKey(PrefService* pref_service,
                                          const url::Origin& origin);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATION_PREFS_UTILS_H_
