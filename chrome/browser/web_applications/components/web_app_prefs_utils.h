// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PREFS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PREFS_UTILS_H_

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

extern const char kWasExternalAppUninstalledByUser[];

extern const char kFileHandlingOriginTrialExpiryTime[];

extern const char kFileHandlersEnabled[];

extern const char kExperimentalTabbedWindowMode[];

extern const char kLatestWebAppInstallSource[];

extern const char kIphIgnoreCount[];

extern const char kIphLastIgnoreTime[];

bool GetBoolWebAppPref(const PrefService* pref_service,
                       const AppId& app_id,
                       base::StringPiece path);

void UpdateBoolWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          bool value);

base::Optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const AppId& app_id,
                                     base::StringPiece path);

void UpdateIntWebAppPref(PrefService* pref_service,
                         const AppId& app_id,
                         base::StringPiece path,
                         int value);

base::Optional<double> GetDoubleWebAppPref(const PrefService* pref_service,
                                           const AppId& app_id,
                                           base::StringPiece path);

void UpdateDoubleWebAppPref(PrefService* pref_service,
                            const AppId& app_id,
                            base::StringPiece path,
                            double value);

base::Optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
                                             const AppId& app_id,
                                             base::StringPiece path);

void UpdateTimeWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          base::Time value);

void RemoveWebAppPref(PrefService* pref_service,
                      const AppId& app_id,
                      base::StringPiece path);

void WebAppPrefsUtilsRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

void RecordInstallIphIgnored(PrefService* pref_service,
                             const AppId& app_id,
                             base::Time time);

void RecordInstallIphInstalled(PrefService* pref_service, const AppId& app_id);

// Returns whether Web App Install In Product Help should be shown based on
// previous interactions with this promo.
bool ShouldShowIph(PrefService* pref_service, const AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PREFS_UTILS_H_
