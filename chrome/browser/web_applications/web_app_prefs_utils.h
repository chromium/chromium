// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREFS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREFS_UTILS_H_

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// TODO(b/313491176): Remove all these public utilities once this utility file
// is retired.
absl::optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const webapps::AppId& app_id,
                                     base::StringPiece path);

void UpdateIntWebAppPref(PrefService* pref_service,
                         const webapps::AppId& app_id,
                         base::StringPiece path,
                         int value);

absl::optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
                                             const webapps::AppId& app_id,
                                             base::StringPiece path);

void UpdateTimeWebAppPref(PrefService* pref_service,
                          const webapps::AppId& app_id,
                          base::StringPiece path,
                          base::Time value);

void RemoveWebAppPref(PrefService* pref_service,
                      const webapps::AppId& app_id,
                      base::StringPiece path);

void WebAppPrefsUtilsRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREFS_UTILS_H_
