// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_prefs_utils.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

const base::Value::Dict* GetWebAppDictionary(const PrefService* pref_service,
                                             const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& web_apps_prefs =
      pref_service->GetDict(prefs::kWebAppsPreferences);

  return web_apps_prefs.FindDict(app_id);
}

base::Value::Dict& UpdateWebAppDictionary(
    ScopedDictPrefUpdate& web_apps_prefs_update,
    const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return *web_apps_prefs_update->EnsureDict(app_id);
}

}  // namespace

absl::optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const webapps::AppId& app_id,
                                     base::StringPiece path) {
  const base::Value::Dict* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs) {
    return web_app_prefs->FindIntByDottedPath(path);
  }
  return absl::nullopt;
}

void UpdateIntWebAppPref(PrefService* pref_service,
                         const webapps::AppId& app_id,
                         base::StringPiece path,
                         int value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, value);
}

absl::optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
                                             const webapps::AppId& app_id,
                                             base::StringPiece path) {
  if (const auto* web_app_prefs = GetWebAppDictionary(pref_service, app_id)) {
    if (auto* value = web_app_prefs->FindByDottedPath(path)) {
      return base::ValueToTime(value);
    }
  }

  return absl::nullopt;
}

void UpdateTimeWebAppPref(PrefService* pref_service,
                          const webapps::AppId& app_id,
                          base::StringPiece path,
                          base::Time value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  auto& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, base::TimeToValue(value));
}

void RemoveWebAppPref(PrefService* pref_service,
                      const webapps::AppId& app_id,
                      base::StringPiece path) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.RemoveByDottedPath(path);
}

// The stored preferences look like:
//   "web_app_ids": {
//     "<app_id_1>": {
//       "was_external_app_uninstalled_by_user": true,
//       "IPH_num_of_consecutive_ignore": 2,
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "IPH_last_ignore_time": "13249617864945580",
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_ignored": "13249617864945580",
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_dismissed": "13249617864945580",
//       "ML_num_of_consecutive_not_accepted": 2,
//       "error_loaded_policy_app_migrated": true
//     },
//   },
//   "app_agnostic_ml_state": {
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_ignored": "13249617864945580",
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "ML_last_time_install_dismissed": "13249617864945580",
//       "ML_num_of_consecutive_not_accepted": 2,
//       "ML_all_promos_blocked_date": "13249617864945580",
//   },
//   "app_agnostic_iph_state": {
//     "IPH_num_of_consecutive_ignore": 3,
//     A string-flavored base::Value representing int64_t number of microseconds
//     since the Windows epoch, using base::TimeToValue().
//     "IPH_last_ignore_time": "13249617864945500",
//   },

void WebAppPrefsUtilsRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(::prefs::kWebAppsPreferences);
  registry->RegisterDictionaryPref(::prefs::kWebAppsAppAgnosticIphState);
  registry->RegisterDictionaryPref(::prefs::kWebAppsAppAgnosticMlState);
  registry->RegisterBooleanPref(::prefs::kShouldGarbageCollectStoragePartitions,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kErrorLoadedPolicyAppMigrationCompleted, false);
}

}  // namespace web_app
