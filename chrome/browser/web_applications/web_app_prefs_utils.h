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
bool TimeOccurredWithinDays(absl::optional<base::Time> time, int days);

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

// -------------------------ML Promotion Guardrails-------------------------
// Pref entries
extern const char kLastTimeMlInstallIgnored[];
extern const char kLastTimeMlInstallDismissed[];
extern const char kConsecutiveMlInstallNotAcceptedCount[];
extern const char kAllMLPromosBlockedTime[];
extern const char kMLPromotionGuardrailBlockReason[];

// Values of all constants required to compute guardrail logic.
extern const int kMuteMlInstallAfterConsecutiveAppSpecificNotAcceptedCount;
extern const int kMuteMlInstallAfterIgnoreForDays;
extern const int kMuteMlInstallAfterDismissForDays;
extern const int kMuteMlInstallAfterConsecutiveAppAgnosticNotAcceptedCount;
extern const int kMuteMlInstallAfterAnyIgnoreForDays;
extern const int kMuteMlInstallAfterAnyDismissForDays;

// The user has ignored the installation dialog and it went away due to
// another interaction (e.g. the tab was changed, page navigated, etc).
void RecordMlInstallIgnored(PrefService* pref_service,
                            const webapps::AppId& app_id,
                            base::Time time);
// The user has taken active action on the dialog to make it go away.
void RecordMlInstallDismissed(PrefService* pref_service,
                              const webapps::AppId& app_id,
                              base::Time time);
void RecordMlInstallAccepted(PrefService* pref_service,
                             const webapps::AppId& app_id,
                             base::Time time);

// Returns true or false based on whether ML promotion has been blocked by
// history guardrails. Since this is triggered whenever Segmentation returns a
// value to show the install prompt, this is a good place to perform
// VerifyAndClearMlGuardrailsIfNeeded.
bool IsMlPromotionBlockedByHistoryGuardrail(PrefService* pref_service,
                                            const webapps::AppId& app_id);

// Resets kAllMLPromosBlockedDate and kConsecutiveMlInstallNotAcceptedCount on
// the app specific and app agnostic levels.
void ResetAllMLPromosBlockedDateAndGuardrails(PrefService* pref_service,
                                              const webapps::AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREFS_UTILS_H_
