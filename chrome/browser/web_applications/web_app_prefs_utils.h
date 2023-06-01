// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREFS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREFS_UTILS_H_

#include <map>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

extern const char kIphIgnoreCount[];

extern const char kIphLastIgnoreTime[];

bool GetBoolWebAppPref(const PrefService* pref_service,
                       const AppId& app_id,
                       base::StringPiece path);

void UpdateBoolWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          bool value);

absl::optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const AppId& app_id,
                                     base::StringPiece path);

void UpdateIntWebAppPref(PrefService* pref_service,
                         const AppId& app_id,
                         base::StringPiece path,
                         int value);

absl::optional<double> GetDoubleWebAppPref(const PrefService* pref_service,
                                           const AppId& app_id,
                                           base::StringPiece path);

void UpdateDoubleWebAppPref(PrefService* pref_service,
                            const AppId& app_id,
                            base::StringPiece path,
                            double value);

absl::optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
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

// Deprecated. See crbug.com/1287292
absl::optional<int> GetWebAppInstallSourceDeprecated(PrefService* prefs,
                                                     const AppId& app_id);

// Looks up all install sources in the web apps prefs dictionary and returns
// them as a map. Also deletes the values from the dictionary. Used for
// migration to the WebApp database. This should be safe to delete one year
// after 02-2022.
std::map<AppId, int> TakeAllWebAppInstallSources(PrefService* prefs);

void RecordInstallIphIgnored(PrefService* pref_service,
                             const AppId& app_id,
                             base::Time time);

void RecordInstallIphInstalled(PrefService* pref_service, const AppId& app_id);

// Returns whether Web App Install In Product Help should be shown based on
// previous interactions with this promo.
bool ShouldShowIph(PrefService* pref_service, const AppId& app_id);

extern const char kLastTimeMlInstallIgnored[];
extern const char kLastTimeMlInstallDismissed[];
extern const char kConsecutiveMlInstallNotAcceptedCount[];

// The user has ignored the installation dialog and it went away due to
// another interaction (e.g. the tab was changed, page navigated, etc).
void RecordMlInstallIgnored(PrefService* pref_service,
                            const AppId& app_id,
                            base::Time time);
// The user has taken active action on the dialog to make it go away.
void RecordMlInstallDismissed(PrefService* pref_service,
                              const AppId& app_id,
                              base::Time time);
void RecordMlInstallAccepted(PrefService* pref_service,
                             const AppId& app_id,
                             base::Time time);

bool IsMlPromotionBlockedByHistoryGuardrail(PrefService* pref_service,
                                            const AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREFS_UTILS_H_
