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
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

const char kLatestWebAppInstallSource[] = "latest_web_app_install_source";

const base::Value::Dict* GetWebAppDictionary(const PrefService* pref_service,
                                             const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& web_apps_prefs =
      pref_service->GetDict(prefs::kWebAppsPreferences);

  return web_apps_prefs.FindDict(app_id);
}

base::Value::Dict& UpdateWebAppDictionary(
    ScopedDictPrefUpdate& web_apps_prefs_update,
    const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return *web_apps_prefs_update->EnsureDict(app_id);
}

// Returns whether the time occurred within X days.
bool TimeOccurredWithinDays(absl::optional<base::Time> time, int days) {
  return time && (base::Time::Now() - time.value()).InDays() < days;
}

// Removes all the empty app ID dictionaries from the `web_app_ids` dictionary.
// That is, this dictionary:
//
//   "web_app_ids": {
//     "<app_id_1>": {},
//     "<app_id_2>": { "foo": true }
//   }
//
// will become this dictionary:
//
//   "web_app_ids": {
//     "<app_id_2>": { "foo": true }
//   }
void RemoveEmptyWebAppPrefs(PrefService* pref_service) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  std::vector<AppId> apps_to_remove;
  for (const auto [app_id, dict] : *update) {
    if (dict.is_dict() && dict.GetDict().empty())
      apps_to_remove.push_back(app_id);
  }

  for (const AppId& app_id : apps_to_remove)
    update->Remove(app_id);
}

}  // namespace

// The stored preferences look like:
//   "web_app_ids": {
//     "<app_id_1>": {
//       "was_external_app_uninstalled_by_user": true,
//       A double representing the number of seconds since epoch, in local time.
//       Convert from/to using base::Time::FromDoubleT() and
//       base::Time::ToDoubleT().
//       "IPH_num_of_consecutive_ignore": 2,
//       A string-flavored base::value representing the int64_t number of
//       microseconds since the Windows epoch, using base::TimeToValue().
//       "IPH_last_ignore_time": "13249617864945580",
//     },
//   },
//   "app_agnostic_iph_state": {
//     "IPH_num_of_consecutive_ignore": 3,
//     A string-flavored base::Value representing int64_t number of microseconds
//     since the Windows epoch, using base::TimeToValue().
//     "IPH_last_ignore_time": "13249617864945500",
//   },
//   isolation_state is managed by isolation_prefs_utils
//   "isolation_state": {
//     "<origin>": {
//       "storage_isolation_key": "abc123",
//     },
//   }
//

const char kIphIgnoreCount[] = "IPH_num_of_consecutive_ignore";

const char kIphLastIgnoreTime[] = "IPH_last_ignore_time";

void WebAppPrefsUtilsRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(::prefs::kWebAppsPreferences);
  registry->RegisterDictionaryPref(::prefs::kWebAppsAppAgnosticIphState);
}

bool GetBoolWebAppPref(const PrefService* pref_service,
                       const AppId& app_id,
                       base::StringPiece path) {
  if (const base::Value::Dict* web_app_prefs =
          GetWebAppDictionary(pref_service, app_id)) {
    return web_app_prefs->FindBoolByDottedPath(path).value_or(false);
  }
  return false;
}

void UpdateBoolWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          bool value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, value);
}

absl::optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const AppId& app_id,
                                     base::StringPiece path) {
  const base::Value::Dict* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs)
    return web_app_prefs->FindIntByDottedPath(path);
  return absl::nullopt;
}

void UpdateIntWebAppPref(PrefService* pref_service,
                         const AppId& app_id,
                         base::StringPiece path,
                         int value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, value);
}

absl::optional<double> GetDoubleWebAppPref(const PrefService* pref_service,
                                           const AppId& app_id,
                                           base::StringPiece path) {
  const base::Value::Dict* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs)
    return web_app_prefs->FindDoubleByDottedPath(path);
  return absl::nullopt;
}

void UpdateDoubleWebAppPref(PrefService* pref_service,
                            const AppId& app_id,
                            base::StringPiece path,
                            double value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, value);
}

absl::optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
                                             const AppId& app_id,
                                             base::StringPiece path) {
  if (const auto* web_app_prefs = GetWebAppDictionary(pref_service, app_id)) {
    if (auto* value = web_app_prefs->FindByDottedPath(path))
      return base::ValueToTime(value);
  }

  return absl::nullopt;
}

void UpdateTimeWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          base::Time value) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  auto& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.SetByDottedPath(path, base::TimeToValue(value));
}

void RemoveWebAppPref(PrefService* pref_service,
                      const AppId& app_id,
                      base::StringPiece path) {
  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsPreferences);

  base::Value::Dict& web_app_prefs = UpdateWebAppDictionary(update, app_id);
  web_app_prefs.RemoveByDottedPath(path);
}

absl::optional<int> GetWebAppInstallSourceDeprecated(PrefService* prefs,
                                                     const AppId& app_id) {
  absl::optional<int> value =
      GetIntWebAppPref(prefs, app_id, kLatestWebAppInstallSource);
  return value;
}

std::map<AppId, int> TakeAllWebAppInstallSources(PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* web_apps_prefs =
      pref_service->GetUserPrefValue(prefs::kWebAppsPreferences);
  if (!web_apps_prefs || !web_apps_prefs->is_dict())
    return {};

  std::map<AppId, int> return_value;
  for (auto item : web_apps_prefs->DictItems()) {
    const AppId& app_id = item.first;
    absl::optional<int> install_source =
        item.second.FindIntPath(kLatestWebAppInstallSource);
    if (install_source)
      return_value.insert(std::make_pair(app_id, *install_source));
  }

  for (const auto& item : return_value)
    RemoveWebAppPref(pref_service, item.first, kLatestWebAppInstallSource);

  RemoveEmptyWebAppPrefs(pref_service);

  return return_value;
}

void RecordInstallIphIgnored(PrefService* pref_service,
                             const AppId& app_id,
                             base::Time time) {
  absl::optional<int> ignored_count =
      GetIntWebAppPref(pref_service, app_id, kIphIgnoreCount);
  int new_count = base::saturated_cast<int>(1 + ignored_count.value_or(0));

  UpdateIntWebAppPref(pref_service, app_id, kIphIgnoreCount, new_count);
  UpdateTimeWebAppPref(pref_service, app_id, kIphLastIgnoreTime, time);

  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsAppAgnosticIphState);
  int global_count = update->FindInt(kIphIgnoreCount).value_or(0);
  update->Set(kIphIgnoreCount, base::saturated_cast<int>(global_count + 1));
  update->Set(kIphLastIgnoreTime, base::TimeToValue(time));
}

void RecordInstallIphInstalled(PrefService* pref_service, const AppId& app_id) {
  // The ignored count is meant to track consecutive occurrences of the user
  // ignoring IPH, to help determine when IPH should be muted. Therefore
  // resetting ignored count on successful install.
  UpdateIntWebAppPref(pref_service, app_id, kIphIgnoreCount, 0);

  ScopedDictPrefUpdate update(pref_service, prefs::kWebAppsAppAgnosticIphState);
  update->Set(kIphIgnoreCount, 0);
}

bool ShouldShowIph(PrefService* pref_service, const AppId& app_id) {
  // Do not show IPH if the user ignored the last N+ promos for this app.
  int app_ignored_count =
      GetIntWebAppPref(pref_service, app_id, kIphIgnoreCount).value_or(0);
  if (app_ignored_count >= kIphMuteAfterConsecutiveAppSpecificIgnores)
    return false;
  // Do not show IPH if the user ignored a promo for this app within N days.
  auto app_last_ignore =
      GetTimeWebAppPref(pref_service, app_id, kIphLastIgnoreTime);
  if (TimeOccurredWithinDays(app_last_ignore,
                             kIphAppSpecificMuteTimeSpanDays)) {
    return false;
  }

  const base::Value::Dict& dict =
      pref_service->GetDict(prefs::kWebAppsAppAgnosticIphState);

  // Do not show IPH if the user ignored the last N+ promos for any app.
  int global_ignored_count = dict.FindInt(kIphIgnoreCount).value_or(0);
  if (global_ignored_count >= kIphMuteAfterConsecutiveAppAgnosticIgnores)
    return false;
  // Do not show IPH if the user ignored a promo for any app within N days.
  auto global_last_ignore = base::ValueToTime(dict.Find(kIphLastIgnoreTime));
  if (TimeOccurredWithinDays(global_last_ignore,
                             kIphAppAgnosticMuteTimeSpanDays)) {
    return false;
  }
  return true;
}

}  // namespace web_app
